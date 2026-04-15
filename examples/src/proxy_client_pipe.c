#include <math.h>
#include <stdint.h>
#include <time.h>
#include <unistd.h>
#include <string.h>
#include <signal.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <sys/stat.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <stdatomic.h>
#include <fcntl.h>

#include "quiet-lwip.h"

#include "quiet-lwip/lwip-socket.h"

#include "relay.h"

const int local_port = 2160;
const int remote_port = 1080;

const uint8_t mac[] = {0x01, 0x02, 0x03, 0x04, 0x05, 0x07};
const quiet_lwip_ipv4_addr ipaddr = (uint32_t)0xc0a80002;   // 192.168.0.2
const char *ipaddr_s = "192.168.0.2";
const quiet_lwip_ipv4_addr netmask = (uint32_t)0xffffff00;  // 255.255.255.0
const quiet_lwip_ipv4_addr gateway = (uint32_t)0xc0a80001;  // 192.168.0.1

typedef struct {
    quiet_lwip_interface *interface;
    int pipe_out_fd;
    int pipe_in_fd;
    size_t sample_size;
    _Atomic bool shutdown;
} pipe_audio_args;

static void write_all(int fd, const void *buf, size_t len) {
    const uint8_t *p = buf;
    while (len > 0) {
        ssize_t n = write(fd, p, len);
        if (n <= 0) return;
        p += n;
        len -= n;
    }
}

static void *pipe_emit_loop(void *arg) {
    pipe_audio_args *args = (pipe_audio_args *)arg;
    quiet_sample_t *buf = malloc(args->sample_size * sizeof(quiet_sample_t));
    // 模拟采样率时钟: 128 samples / 48000 Hz ≈ 2667 us
    long interval_ns = (long)(args->sample_size * 1000000000LL / 48000);
    struct timespec next;
    clock_gettime(CLOCK_MONOTONIC, &next);
    for (;;) {
        ssize_t n = quiet_lwip_get_next_audio_packet(
            args->interface, buf, args->sample_size);
        if (n > 0) {
            write_all(args->pipe_out_fd, buf, n * sizeof(quiet_sample_t));
        } else if (atomic_load(&args->shutdown)) {
            break;
        }
        next.tv_nsec += interval_ns;
        if (next.tv_nsec >= 1000000000L) {
            next.tv_sec++;
            next.tv_nsec -= 1000000000L;
        }
        clock_nanosleep(CLOCK_MONOTONIC, TIMER_ABSTIME, &next, NULL);
    }
    free(buf);
    pthread_exit(NULL);
}

static void *pipe_recv_loop(void *arg) {
    pipe_audio_args *args = (pipe_audio_args *)arg;
    size_t buf_bytes = args->sample_size * sizeof(quiet_sample_t);
    quiet_sample_t *buf = malloc(buf_bytes);
    for (;;) {
        if (atomic_load(&args->shutdown)) break;
        ssize_t n = read(args->pipe_in_fd, buf, buf_bytes);
        if (n <= 0) {
            if (atomic_load(&args->shutdown)) break;
            continue;
        }
        quiet_lwip_recv_audio_packet(
            args->interface, buf, n / sizeof(quiet_sample_t));
    }
    free(buf);
    pthread_exit(NULL);
}

int open_send(const char *addr) {
    int socket_fd = lwip_socket(AF_INET, SOCK_STREAM, 0);

    if (socket_fd < 0) {
        printf("socket failed\n");
        return -1;
    }

    struct lwip_sockaddr_in remote;
    remote.sin_family = AF_INET;
    remote.sin_addr.s_addr = inet_addr(addr);
    remote.sin_port = htons(remote_port);
    int res = lwip_connect(socket_fd, (struct lwip_sockaddr*)&remote, sizeof(remote));

    if (res < 0) {
        printf("connect failed\n");
        lwip_close(socket_fd);
        return -1;
    }

    return socket_fd;
}

int open_recv(const char *addr) {
    int socket_fd = socket(AF_INET, SOCK_STREAM, 0);

    if (socket_fd < 0) {
        printf("socket failed\n");
        return -1;
    }

    int reuse = 1;
    if (setsockopt(socket_fd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse)) < 0) {
        printf("setsockopt SO_REUSEADDR failed\n");
        close(socket_fd);
        return -1;
    }

    struct sockaddr_in *local_addr = calloc(1, sizeof(struct sockaddr_in));
    local_addr->sin_family = AF_INET;
    local_addr->sin_addr.s_addr = inet_addr(addr);
    local_addr->sin_port = htons(local_port);

    int res = bind(socket_fd, (struct sockaddr *)local_addr, sizeof(struct sockaddr_in));
    free(local_addr);

    if (res < 0) {
        printf("bind failed\n");
        return -1;
    }

    res = listen(socket_fd, 1);

    if (res < 0) {
        printf("listen failed\n");
        return -1;
    }

    return socket_fd;
}

int recv_connection(int socket_fd, struct sockaddr_in *recv_from) {
    socklen_t recv_from_len = sizeof(recv_from);
    return accept(socket_fd, (struct sockaddr *)recv_from, &recv_from_len);
}

int main(int argc, char **argv) {
    const char *profile_key = "cable-64k";
    const char *conf_path = NULL;
    const char *proxy_addr = NULL;
    const char *pipe_out_path = NULL;
    const char *pipe_in_path = NULL;
    unsigned int sample_rate = 48000;

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--help") == 0) {
            printf("Usage: %s <proxy_addr> --pipe-out <path> --pipe-in <path> [options]\n\n"
                   "Arguments:\n"
                   "  proxy_addr                IPv4 address of the proxy to connect to\n"
                   "Options:\n"
                   "  --pipe-out <path>         named pipe for audio output (encoder → peer)\n"
                   "  --pipe-in <path>          named pipe for audio input (peer → decoder)\n"
                   "  --conf <path>             path to quiet-profiles.json\n"
                   "  --profile <name>          quiet profile to use (default: cable-64k)\n"
                   "  --sample-rate <hz>        sample rate (default: 48000)\n"
                   "  --help                    show this help message\n", argv[0]);
            return 0;
        }
    }
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--conf") == 0) {
            if (i + 1 >= argc) { printf("--conf requires an argument\n"); return 1; }
            conf_path = argv[++i];
        } else if (strcmp(argv[i], "--profile") == 0) {
            if (i + 1 >= argc) { printf("--profile requires an argument\n"); return 1; }
            profile_key = argv[++i];
        } else if (strcmp(argv[i], "--pipe-out") == 0) {
            if (i + 1 >= argc) { printf("--pipe-out requires an argument\n"); return 1; }
            pipe_out_path = argv[++i];
        } else if (strcmp(argv[i], "--pipe-in") == 0) {
            if (i + 1 >= argc) { printf("--pipe-in requires an argument\n"); return 1; }
            pipe_in_path = argv[++i];
        } else if (strcmp(argv[i], "--sample-rate") == 0) {
            if (i + 1 >= argc) { printf("--sample-rate requires an argument\n"); return 1; }
            sample_rate = atoi(argv[++i]);
        } else if (argv[i][0] != '-') {
            if (!proxy_addr) {
                proxy_addr = argv[i];
            }
        }
    }
    if (!proxy_addr || !pipe_out_path || !pipe_in_path) {
        printf("Usage: %s <proxy_addr> --pipe-out <path> --pipe-in <path>\n", argv[0]);
        return 1;
    }

    signal(SIGPIPE, SIG_IGN);

    quiet_lwip_driver_config *conf = calloc(1, sizeof(quiet_lwip_driver_config));
    conf->encoder_opt = quiet_encoder_profile_filename(conf_path, profile_key);
    if (!conf->encoder_opt) {
        printf("failed to read encoder profile '%s' (path: %s)\n",
               profile_key, conf_path ? conf_path : "default");
        free(conf);
        return 1;
    }
    conf->decoder_opt = quiet_decoder_profile_filename(conf_path, profile_key);
    if (!conf->decoder_opt) {
        printf("failed to read decoder profile '%s' (path: %s)\n",
               profile_key, conf_path ? conf_path : "default");
        free(conf);
        return 1;
    }
    conf->encoder_rate = sample_rate;
    conf->decoder_rate = sample_rate;
    memcpy(conf->hardware_addr, mac, 6);

    quiet_lwip_interface *interface =
        quiet_lwip_create(conf, htonl(ipaddr), htonl(netmask), htonl(gateway));
    free(conf);

    if (!interface) {
        printf("failed to create quiet lwip interface\n");
        return 1;
    }

    printf("opening pipe-out: %s\n", pipe_out_path);
    int pipe_out_fd = open(pipe_out_path, O_WRONLY);
    if (pipe_out_fd < 0) {
        perror("failed to open pipe-out");
        return 1;
    }
    printf("opening pipe-in: %s\n", pipe_in_path);
    int pipe_in_fd = open(pipe_in_path, O_RDONLY);
    if (pipe_in_fd < 0) {
        perror("failed to open pipe-in");
        close(pipe_out_fd);
        return 1;
    }
    printf("pipes opened\n");

    pipe_audio_args emit_args = {
        .interface = interface,
        .pipe_out_fd = pipe_out_fd,
        .pipe_in_fd = -1,
        .sample_size = 1 << 7,
        .shutdown = false,
    };
    pipe_audio_args recv_args = {
        .interface = interface,
        .pipe_out_fd = -1,
        .pipe_in_fd = pipe_in_fd,
        .sample_size = 1 << 7,
        .shutdown = false,
    };

    pthread_t emit_thread, recv_thread;
    pthread_create(&emit_thread, NULL, pipe_emit_loop, &emit_args);
    pthread_create(&recv_thread, NULL, pipe_recv_loop, &recv_args);

    crossbar client_crossbar;
    crossbar remote_crossbar;
    crossbar_init(&client_crossbar);
    crossbar_init(&remote_crossbar);

    relay_t client_relay = {
        .agent = agent_native,
        .other_agent = agent_lwip,
        .incoming = &client_crossbar,
        .outgoing = &remote_crossbar,
        .read = read,
        .write = write,
        .select = select,
        .other_shutdown = lwip_shutdown,
        .get_errno = native_errno,
    };

    relay_t remote_relay = {
        .agent = agent_lwip,
        .other_agent = agent_native,
        .incoming = &remote_crossbar,
        .outgoing = &client_crossbar,
        .read = _lwip_read,
        .write = _lwip_write,
        .select = lwip_select,
        .other_shutdown = shutdown,
        .get_errno = lwip_errno,
    };

    start_relay_thread(&client_relay);
    start_relay_thread(&remote_relay);

    int recv_socket = open_recv("127.0.0.1");

    if (recv_socket < 0) {
        printf("couldn't open socket for listening\n");
        exit(1);
    }

    for (;;) {
        struct sockaddr_in recv_from;
        int conn_fd = recv_connection(recv_socket, &recv_from);
        if (conn_fd < 0) {
            continue;
        }

        printf("received connection from %s\n", inet_ntoa(recv_from.sin_addr));

        int remote_fd = open_send(proxy_addr);

        if (remote_fd < 0) {
            close(conn_fd);
            printf("remote connect failed\n");
            continue;
        }

        relay_conn *conn = relay_conn_create(conn_fd, remote_fd, 1 << 13);

        crossbar_add_for_reading(&client_crossbar, conn);
        crossbar_add_for_reading(&remote_crossbar, conn);
    }

    atomic_store(&emit_args.shutdown, true);
    atomic_store(&recv_args.shutdown, true);
    close(pipe_out_fd);
    close(pipe_in_fd);
    pthread_join(emit_thread, NULL);
    pthread_join(recv_thread, NULL);
    quiet_lwip_destroy(interface);

    return 0;
}
