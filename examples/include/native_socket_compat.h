#ifndef NATIVE_SOCKET_COMPAT_H
#define NATIVE_SOCKET_COMPAT_H

#include <stddef.h>
#include <stdint.h>
#include <sys/types.h>

#ifdef _WIN32

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#include <errno.h>

#ifndef SHUT_RD
#define SHUT_RD SD_RECEIVE
#endif
#ifndef SHUT_WR
#define SHUT_WR SD_SEND
#endif
#ifndef SHUT_RDWR
#define SHUT_RDWR SD_BOTH
#endif

#ifndef EWOULDBLOCK
#define EWOULDBLOCK WSAEWOULDBLOCK
#endif

static inline int native_close(int fd) { return closesocket((SOCKET)fd); }

static inline int native_set_nonblock(int fd) {
    u_long mode = 1;
    return ioctlsocket((SOCKET)fd, FIONBIO, &mode);
}

static inline int native_get_errno(void) {
    int e = WSAGetLastError();
    if (e == WSAEWOULDBLOCK) return EWOULDBLOCK;
    if (e == WSAEINTR) return EINTR;
    return e;
}

static inline ssize_t native_read(int fd, void *buf, size_t n) {
    int r = recv((SOCKET)fd, (char *)buf, (int)n, 0);
    return (ssize_t)r;
}

static inline ssize_t native_write(int fd, const void *buf, size_t n) {
    int r = send((SOCKET)fd, (const char *)buf, (int)n, 0);
    return (ssize_t)r;
}

static inline int native_shutdown(int fd, int how) {
    return shutdown((SOCKET)fd, how);
}

static inline int native_select(int nfds, fd_set *r, fd_set *w, fd_set *e,
                                struct timeval *tv) {
    (void)nfds;
    return select(0, r, w, e, tv);
}

#else /* POSIX */

#include <unistd.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <errno.h>

static inline int native_close(int fd) { return close(fd); }

static inline int native_set_nonblock(int fd) {
    return fcntl(fd, F_SETFL, O_NONBLOCK);
}

static inline int native_get_errno(void) { return errno; }

static inline ssize_t native_read(int fd, void *buf, size_t n) {
    return read(fd, buf, n);
}

static inline ssize_t native_write(int fd, const void *buf, size_t n) {
    return write(fd, buf, n);
}

static inline int native_shutdown(int fd, int how) {
    return shutdown(fd, how);
}

static inline int native_select(int nfds, fd_set *r, fd_set *w, fd_set *e,
                                struct timeval *tv) {
    return select(nfds, r, w, e, tv);
}

#endif /* _WIN32 */

#endif /* NATIVE_SOCKET_COMPAT_H */
