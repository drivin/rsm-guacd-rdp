/*
 * Windows compatibility stub for netdb.h
 *
 * On Windows, getaddrinfo / freeaddrinfo / getnameinfo / struct addrinfo /
 * struct hostent / NI_* / AI_* are all provided by <ws2tcpip.h>.
 */

#ifndef COMPAT_NETDB_H
#define COMPAT_NETDB_H

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>

/* gai_strerror() is available in ws2tcpip.h on Windows Vista+.
 * Older mingw-w64 headers may not expose it, so provide a fallback. */
#ifndef gai_strerror
static inline const char* gai_strerror(int ecode) {
    (void)ecode;
    return "name resolution error";
}
#endif

#else  /* !_WIN32 */
#include_next <netdb.h>
#endif

#endif /* COMPAT_NETDB_H */
