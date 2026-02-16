#include <math.h>
#include <stdint.h>
#include <time.h>
#include <unistd.h>
#include <pthread.h>
#include <stdatomic.h>

#include "quiet-lwip-portaudio.h"

#include "lwip/sockets.h"

const int remote_port = 7173;

const uint8_t mac[] = {0x01, 0x02, 0x03, 0x04, 0x05, 0x06};
const quiet_lwip_ipv4_addr ipaddr = (uint32_t)0xc0a80002;   // 192.168.0.2
const char *ipaddr_s = "192.168.0.2";
const quiet_lwip_ipv4_addr netmask = (uint32_t)0xffffff00;  // 255.255.255.0
const quiet_lwip_ipv4_addr gateway = (uint32_t)0xc0a80001;  // 192.168.0.1

int open_send(const char *addr) {
    int socket_fd = socket(AF_INET, SOCK_STREAM, 0);

    if (socket_fd < 0) {
        printf("socket failed\n");
        return -1;
    }

    struct sockaddr_in remote;
    remote.sin_family = AF_INET;
    remote.sin_addr.s_addr = inet_addr(addr);
    remote.sin_port = htons(remote_port);
    int res = connect(socket_fd, (struct sockaddr*)&remote, sizeof(remote));

    if (res < 0) {
        printf("connect failed\n");
    }

    return socket_fd;
}

int main(int argc, char **argv) {
    const char *encoder_key = "audible-7k-channel-0";
    const char *decoder_key = "audible-7k-channel-1";
    const char *conf_path = NULL;
    const char *server_addr = NULL;
    const char *request = NULL;

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--help") == 0) {
            printf("Usage: %s <server_addr> <request_string> [--conf <path>] [--enc-profile <name>] [--dec-profile <name>]\n\n"
                   "Arguments:\n"
                   "  server_addr               IPv4 address of the kv server\n"
                   "  request_string            The command to send (e.g., PING, ADD:key=value, GET:key)\n\n"
                   "Options:\n"
                   "  --conf <path>             path to quiet-profiles.json\n"
                   "  --enc-profile <name>      encoder profile to use (default: audible-7k-channel-0)\n"
                   "  --dec-profile <name>      decoder profile to use (default: audible-7k-channel-1)\n"
                   "  --help                    show this help message\n", argv[0]);
            return 0;
        }
    }

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--conf") == 0) {
            if (i + 1 >= argc) {
                printf("--conf requires an argument\n");
                return 1;
            }
            conf_path = argv[++i];
        } else if (strcmp(argv[i], "--enc-profile") == 0) {
            if (i + 1 >= argc) {
                printf("--enc-profile requires an argument\n");
                return 1;
            }
            encoder_key = argv[++i];
        } else if (strcmp(argv[i], "--dec-profile") == 0) {
            if (i + 1 >= argc) {
                printf("--dec-profile requires an argument\n");
                return 1;
            }
            decoder_key = argv[++i];
        } else if (argv[i][0] != '-') {
            if (!server_addr) {
                server_addr = argv[i];
            } else if (!request) {
                request = argv[i];
            }
        }
    }

    if (!server_addr || !request) {
        printf("Usage: %s <server_addr> <request_string> [--conf <path>]\n", argv[0]);
        return 1;
    }

    PaError err = Pa_Initialize();
    if (err != paNoError) {
        printf("failed to initialize port audio, %s\n", Pa_GetErrorText(err));
        return 1;
    }

    quiet_lwip_portaudio_driver_config *conf =
        calloc(1, sizeof(quiet_lwip_portaudio_driver_config));
    conf->encoder_opt =
        quiet_encoder_profile_filename(conf_path, encoder_key);
    if (!conf->encoder_opt) {
        printf("failed to read encoder profile '%s'\n", encoder_key);
        free(conf);
        return 1;
    }
    conf->decoder_opt =
        quiet_decoder_profile_filename(conf_path, decoder_key);
    if (!conf->decoder_opt) {
        printf("failed to read decoder profile '%s'\n", decoder_key);
        free(conf);
        return 1;
    }

    conf->encoder_device = Pa_GetDefaultOutputDevice();
    if (conf->encoder_device == paNoDevice) {
        printf("no default output device found\n");
        free(conf);
        return 1;
    }
    const PaDeviceInfo *device_info = Pa_GetDeviceInfo(conf->encoder_device);
    if (!device_info) {
        printf("failed to get output device info\n");
        free(conf);
        return 1;
    }
    conf->encoder_sample_rate = device_info->defaultSampleRate;
    conf->encoder_latency = device_info->defaultLowOutputLatency;

    conf->decoder_device = Pa_GetDefaultInputDevice();
    if (conf->decoder_device == paNoDevice) {
        printf("no default input device found\n");
        free(conf);
        return 1;
    }
    device_info = Pa_GetDeviceInfo(conf->decoder_device);
    if (!device_info) {
        printf("failed to get input device info\n");
        free(conf);
        return 1;
    }
    conf->decoder_sample_rate = device_info->defaultSampleRate;
    conf->decoder_latency = device_info->defaultLowOutputLatency;

    conf->encoder_sample_size = 1 << 8;
    conf->decoder_sample_size = 1 << 8;

    memcpy(conf->hardware_addr, mac, 6);
    quiet_lwip_portaudio_interface *interface =
        quiet_lwip_portaudio_create(conf, htonl(ipaddr), htonl(netmask), htonl(gateway));
    free(conf);

    if (!interface) {
        printf("failed to create quiet lwip portaudio interface\n");
        Pa_Terminate();
        return 1;
    }

    quiet_lwip_portaudio_audio_threads *audio_threads =
        quiet_lwip_portaudio_start_audio_threads(interface);

    size_t buf_len = 4096;
    uint8_t *buf = calloc(buf_len, sizeof(uint8_t));

    int send_socket = open_send(server_addr);

    memcpy(buf, request, strlen(request));

    ssize_t write_len = write(send_socket, buf, strlen(request));

    if (write_len < 0) {
        printf("write to socket failed\n");
    }

    memset(buf, 0, buf_len);

    ssize_t recv_len = read(send_socket, buf, buf_len);
    printf("%.*s\n", (int)recv_len, buf);
    close(send_socket);

    sleep(1);

    quiet_lwip_portaudio_stop_audio_threads(audio_threads);
    free(buf);
    quiet_lwip_portaudio_destroy(interface);

    Pa_Terminate();

    return 0;
}
