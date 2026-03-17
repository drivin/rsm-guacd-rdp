/*
 * Windows POSIX compatibility stubs for guacd.
 *
 * Provides stubs for POSIX functions and constants that are not available
 * in MINGW64/Windows:
 *   - fork(), setsid(), setpgid()
 *   - kill(), waitpid()
 *   - sigaction, SIGPIPE, SIGCHLD
 *   - socketpair() via TCP loopback
 */

#ifndef COMPAT_WINDOWS_POSIX_H
#define COMPAT_WINDOWS_POSIX_H

#ifdef _WIN32

#include <errno.h>
#include <signal.h>
#include <stdarg.h>
#include <sys/types.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#include <string.h>
#include <stdio.h>

/* -----------------------------------------------------------------------
 * WinSock auto-initialization
 *
 * WSAStartup must be called before any socket operation (including
 * getaddrinfo). Use __attribute__((constructor)) to run before main().
 * Each TU compiled with this header registers one constructor; multiple
 * WSAStartup calls are safe (reference-counted by WinSock).
 * --------------------------------------------------------------------- */
static void __attribute__((constructor)) _win32_wsa_init(void) {
    WSADATA _wsa;
    WSAStartup(MAKEWORD(2, 2), &_wsa);
}

/* -----------------------------------------------------------------------
 * Signal numbers missing on Windows
 * --------------------------------------------------------------------- */

#ifndef SIGPIPE
#define SIGPIPE 13
#endif

#ifndef SIGCHLD
#define SIGCHLD 17
#endif

#ifndef SIGHUP
#define SIGHUP  1
#endif

#ifndef SIGKILL
#define SIGKILL 9
#endif

/* srandom / random → srand/rand (Windows only has srand/rand) */
#ifndef srandom
#define srandom(x) srand((unsigned int)(x))
#endif
#ifndef random
#define random() rand()
#endif

/* -----------------------------------------------------------------------
 * setenv / unsetenv — use SetEnvironmentVariable
 * --------------------------------------------------------------------- */

#ifndef _SETENV_WIN32_STUB
#define _SETENV_WIN32_STUB
static inline int setenv(const char *name, const char *value, int overwrite) {
    if (!overwrite && getenv(name) != NULL) return 0;
    return SetEnvironmentVariableA(name, value) ? 0 : -1;
}
static inline int unsetenv(const char *name) {
    return SetEnvironmentVariableA(name, NULL) ? 0 : -1;
}
#endif /* _SETENV_WIN32_STUB */

/* -----------------------------------------------------------------------
 * sigaction stub (maps to signal())
 * --------------------------------------------------------------------- */

#ifndef _SIGACTION_DEFINED
#define _SIGACTION_DEFINED

typedef void (*sighandler_t)(int);

#ifndef sigset_t
typedef unsigned long sigset_t;
#endif

struct sigaction {
    sighandler_t sa_handler;
    sigset_t     sa_mask;
    int          sa_flags;
    void       (*sa_sigaction)(int, void*, void*);
};

static inline int sigaction(int sig, const struct sigaction* act,
                             struct sigaction* old) {
    if (old != NULL) {
        sighandler_t prev = signal(sig, SIG_DFL);
        old->sa_handler = prev;
        old->sa_mask    = 0;
        old->sa_flags   = 0;
        /* Restore previous handler */
        signal(sig, prev);
    }
    if (act != NULL) {
        signal(sig, act->sa_handler);
    }
    return 0;
}
#endif /* _SIGACTION_DEFINED */

/* -----------------------------------------------------------------------
 * Process / privilege stubs
 *
 * GCC knows fork/waitpid/kill/setsid/setpgid as built-ins with specific
 * return types (usually 'int').  On MINGW64, pid_t may be 'short', causing
 * a [-Wbuiltin-declaration-mismatch] error.  Suppress it around our stubs.
 * --------------------------------------------------------------------- */

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wbuiltin-declaration-mismatch"

/* fork() - always fails on Windows; guacd must be run with -f (foreground) */
static inline pid_t fork(void) {
    errno = ENOSYS;
    return -1;
}

/* setsid() - no-op on Windows */
static inline pid_t setsid(void) {
    return (pid_t)GetCurrentProcessId();
}

/* setpgid() - no-op on Windows */
static inline int setpgid(pid_t pid, pid_t pgid) {
    (void)pid; (void)pgid;
    return 0;
}

/* getpgid() - stub */
static inline pid_t getpgid(pid_t pid) {
    (void)pid;
    return (pid_t)GetCurrentProcessId();
}

/* kill() - stub (can't send signals across processes on Windows) */
static inline int kill(pid_t pid, int sig) {
    (void)pid; (void)sig;
    return 0;
}

/* waitpid() - stub; always returns ECHILD */
static inline pid_t waitpid(pid_t pid, int* status, int options) {
    (void)pid; (void)options;
    if (status) *status = 0;
    errno = ECHILD;
    return -1;
}

#pragma GCC diagnostic pop

/* -----------------------------------------------------------------------
 * socketpair() emulation via TCP loopback
 *
 * Creates two connected SOCK_STREAM sockets on 127.0.0.1.
 * The SOCKET values are cast to int (valid for small handle indices
 * that WinSock uses in practice).
 * --------------------------------------------------------------------- */

static inline int socketpair(int domain, int type, int protocol, int sv[2]) {
    (void)domain; (void)type; (void)protocol;

    struct sockaddr_in addr;
    int addrlen = sizeof(addr);

    SOCKET listener = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (listener == INVALID_SOCKET) return -1;

    memset(&addr, 0, sizeof(addr));
    addr.sin_family      = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    addr.sin_port        = 0;

    if (bind(listener, (struct sockaddr*)&addr, sizeof(addr)) == SOCKET_ERROR)
        goto err_listener;
    if (listen(listener, 1) == SOCKET_ERROR)
        goto err_listener;
    if (getsockname(listener, (struct sockaddr*)&addr, &addrlen) == SOCKET_ERROR)
        goto err_listener;

    SOCKET s0 = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (s0 == INVALID_SOCKET) goto err_listener;

    if (connect(s0, (struct sockaddr*)&addr, sizeof(addr)) == SOCKET_ERROR) {
        closesocket(s0);
        goto err_listener;
    }

    SOCKET s1 = accept(listener, NULL, NULL);
    if (s1 == INVALID_SOCKET) {
        closesocket(s0);
        goto err_listener;
    }

    closesocket(listener);
    sv[0] = (int)(intptr_t)s0;
    sv[1] = (int)(intptr_t)s1;
    return 0;

err_listener:
    closesocket(listener);
    return -1;
}

/* -----------------------------------------------------------------------
 * AF_UNIX / SOCK_DGRAM constants that connection.c / proc.c reference
 * --------------------------------------------------------------------- */

#ifndef AF_UNIX
#define AF_UNIX AF_INET
#endif

#ifndef SHUT_RDWR
#define SHUT_RDWR SD_BOTH
#endif

/* -----------------------------------------------------------------------
 * errno values missing on Windows
 * --------------------------------------------------------------------- */

/* EBADFD is a Linux extension; EBADF is the closest Windows equivalent */
#ifndef EBADFD
#define EBADFD EBADF
#endif

/* -----------------------------------------------------------------------
 * fcntl() stub — supports F_GETFL / F_SETFL with O_NONBLOCK only.
 *
 * tcp.c uses fcntl to put a socket into non-blocking mode:
 *   opt = fcntl(fd, F_GETFL, NULL);
 *   fcntl(fd, F_SETFL, opt | O_NONBLOCK);
 *
 * On Windows the equivalent is ioctlsocket(fd, FIONBIO, &mode).
 * F_GETFL returns a fake flags word of 0 (not O_NONBLOCK) so the
 * subsequent F_SETFL | O_NONBLOCK always sets non-blocking mode.
 * --------------------------------------------------------------------- */

#ifndef F_GETFL
#define F_GETFL  3
#endif
#ifndef F_SETFL
#define F_SETFL  4
#endif
#ifndef O_NONBLOCK
#define O_NONBLOCK 0x0004
#endif

/* -----------------------------------------------------------------------
 * fdopendir(): open directory stream from a file descriptor.
 * Not available on MINGW64. Get the path via GetFinalPathNameByHandle
 * and delegate to opendir().
 * --------------------------------------------------------------------- */

#include <dirent.h>
#include <io.h>

#ifndef _FDOPENDIR_WIN32_STUB
#define _FDOPENDIR_WIN32_STUB
static inline DIR* fdopendir(int fd) {
    HANDLE h = (HANDLE)_get_osfhandle(fd);
    if (h == INVALID_HANDLE_VALUE) { errno = EBADF; return NULL; }

    char buf[MAX_PATH + 4];
    DWORD len = GetFinalPathNameByHandleA(h, buf, (DWORD)sizeof(buf),
                                          FILE_NAME_NORMALIZED);
    if (len == 0 || len >= (DWORD)sizeof(buf)) { errno = ENOENT; return NULL; }

    /* GetFinalPathNameByHandleA prepends \\?\ — strip it for opendir() */
    char *path = buf;
    if (strncmp(path, "\\\\?\\", 4) == 0) path += 4;
    return opendir(path);
}
#endif /* _FDOPENDIR_WIN32_STUB */

/* -----------------------------------------------------------------------
 * mkdir: Windows version takes only 1 argument (no permission mode).
 * Map the 2-argument POSIX call to _mkdir() from <direct.h>.
 * --------------------------------------------------------------------- */

#include <direct.h>
#ifndef _MKDIR_WIN32_STUB
#define _MKDIR_WIN32_STUB
/* Undefine any existing macro first to avoid redefinition warnings */
#ifdef mkdir
#undef mkdir
#endif
#define mkdir(path, mode) _mkdir(path)
#endif /* _MKDIR_WIN32_STUB */

/* -----------------------------------------------------------------------
 * pipe(): Windows equivalent is _pipe() from <io.h>.
 * --------------------------------------------------------------------- */

#include <io.h>
#ifndef _O_BINARY
#define _O_BINARY 0x8000
#endif
#ifndef _PIPE_WIN32_STUB
#define _PIPE_WIN32_STUB
#ifdef pipe
#undef pipe
#endif
#define pipe(fds) _pipe((fds), 65536, _O_BINARY)
#endif /* _PIPE_WIN32_STUB */

/* -----------------------------------------------------------------------
 * wcwidth(): returns the display-column width of a Unicode code point.
 * Not available on Windows. Simplified stub sufficient for the terminal
 * library (which is compiled but unused in an RDP-only build).
 * --------------------------------------------------------------------- */

#include <wchar.h>
#ifndef _WCWIDTH_DEFINED
#define _WCWIDTH_DEFINED
static inline int wcwidth(unsigned int ucs) {
    if (ucs == 0) return 0;
    /* C0/C1 control characters */
    if (ucs < 32 || (ucs >= 0x7f && ucs < 0xa0)) return -1;
    /* Double-width CJK ranges (simplified) */
    if ((ucs >= 0x1100  && ucs <= 0x115f)  ||
        (ucs >= 0x2e80  && ucs <= 0x303e)  ||
        (ucs >= 0x3041  && ucs <= 0x33ff)  ||
        (ucs >= 0xac00  && ucs <= 0xd7a3)  ||
        (ucs >= 0xff01  && ucs <= 0xff60)  ||
        (ucs >= 0xffe0  && ucs <= 0xffe6)  ||
        (ucs >= 0x1f300 && ucs <= 0x1f9ff))
        return 2;
    return 1;
}
#endif /* _WCWIDTH_DEFINED */

#ifndef _COMPAT_FCNTL_DEFINED
#define _COMPAT_FCNTL_DEFINED
static inline int fcntl(int fd, int cmd, ...) {
    if (cmd == F_GETFL) {
        return 0; /* pretend current flags = 0 (blocking) */
    }
    if (cmd == F_SETFL) {
        va_list ap;
        va_start(ap, cmd);
        int flags = va_arg(ap, int);
        va_end(ap);
        u_long mode = (flags & O_NONBLOCK) ? 1 : 0;
        return ioctlsocket((SOCKET)(uintptr_t)(unsigned int)fd,
                           FIONBIO, &mode) == 0 ? 0 : -1;
    }
    errno = EINVAL;
    return -1;
}
#endif /* _COMPAT_FCNTL_DEFINED */

#endif /* _WIN32 */
#endif /* COMPAT_WINDOWS_POSIX_H */
