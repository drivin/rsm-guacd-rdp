/*
 * Windows compatibility stub for arpa/inet.h
 * inet_ntop / inet_pton / htons / ntohs / htonl / ntohl are in ws2tcpip.h.
 */

#ifndef COMPAT_ARPA_INET_H
#define COMPAT_ARPA_INET_H

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include_next <arpa/inet.h>
#endif

#endif /* COMPAT_ARPA_INET_H */
