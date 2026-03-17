/*
 * Windows compatibility stub for <netinet/tcp.h>
 * TCP_NODELAY and related constants come from <winsock2.h>.
 */

#ifndef COMPAT_NETINET_TCP_H
#define COMPAT_NETINET_TCP_H

#ifdef _WIN32
#include <winsock2.h>
/* TCP_NODELAY is already defined in winsock2.h */
#ifndef TCP_NODELAY
#define TCP_NODELAY 0x0001
#endif
#else
#include_next <netinet/tcp.h>
#endif

#endif /* COMPAT_NETINET_TCP_H */
