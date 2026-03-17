/*
 * Windows compatibility stub for sys/socket.h
 * All socket types, constants, and functions are in winsock2.h / ws2tcpip.h.
 */

#ifndef COMPAT_SYS_SOCKET_H
#define COMPAT_SYS_SOCKET_H

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>

/* POSIX sockopts / levels not defined by winsock */
#ifndef SOL_SOCKET
#define SOL_SOCKET  0xffff
#endif

#ifndef AF_UNIX
#define AF_UNIX     1
#endif

/* ssize_t */
#ifndef _SSIZE_T_DEFINED
#define _SSIZE_T_DEFINED
typedef long long ssize_t;
#endif

#else  /* !_WIN32 */
#include_next <sys/socket.h>
#endif

#endif /* COMPAT_SYS_SOCKET_H */
