/*
 * Windows compatibility shim for <fcntl.h>
 *
 * Placed first in the include path (-I compat-win32) so the compiler finds
 * this file before the system one.
 *
 * Uses #include_next (a GCC extension) to pull in the real system fcntl.h
 * first, then adds POSIX symbols that MINGW64's fcntl.h omits:
 *   F_GETFL / F_SETFL / O_NONBLOCK / fcntl()
 *   EBADFD  (Linux errno extension used by tcp.c)
 *
 * Build must pass -Wno-pedantic (done in build.sh CFLAGS) so that GCC does
 * not treat #include_next as an error under -Wpedantic.
 */

#ifndef COMPAT_FCNTL_H
#define COMPAT_FCNTL_H

/* Pull in the real system fcntl.h (O_RDONLY, O_CREAT, …) */
#include_next <fcntl.h>      /* GCC extension — requires -Wno-pedantic */

#ifdef _WIN32
#include <winsock2.h>
#include <stdarg.h>
#include <errno.h>

#ifndef F_GETFL
#define F_GETFL  3
#endif
#ifndef F_SETFL
#define F_SETFL  4
#endif
#ifndef O_NONBLOCK
#define O_NONBLOCK 0x0004
#endif
#ifndef EBADFD
#define EBADFD EBADF
#endif

#ifndef _COMPAT_FCNTL_DEFINED
#define _COMPAT_FCNTL_DEFINED
static inline int fcntl(int fd, int cmd, ...) {
    if (cmd == F_GETFL)
        return 0;
    if (cmd == F_SETFL) {
        va_list ap;
        va_start(ap, cmd);
        int flags = va_arg(ap, int);
        va_end(ap);
        u_long nonblocking = (flags & O_NONBLOCK) ? 1 : 0;
        return ioctlsocket((SOCKET)(uintptr_t)(unsigned int)fd,
                           FIONBIO, &nonblocking) == 0 ? 0 : -1;
    }
    errno = EINVAL;
    return -1;
}
#endif /* _COMPAT_FCNTL_DEFINED */

#endif /* _WIN32 */
#endif /* COMPAT_FCNTL_H */
