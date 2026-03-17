/*
 * Windows compatibility stub for netinet/in.h
 *
 * On Windows, struct in_addr / struct sockaddr_in / IPPROTO_* / in_port_t /
 * in_addr_t / htons / ntohs / htonl / ntohl are all provided by winsock2.h
 * and ws2tcpip.h.
 */

#ifndef COMPAT_NETINET_IN_H
#define COMPAT_NETINET_IN_H

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>

/* POSIX type aliases not always defined by winsock headers */
#ifndef in_port_t
typedef unsigned short in_port_t;
#endif
#ifndef in_addr_t
typedef unsigned long  in_addr_t;
#endif

#else  /* !_WIN32 */
#include_next <netinet/in.h>
#endif

#endif /* COMPAT_NETINET_IN_H */
