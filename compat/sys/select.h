/*
 * Windows compatibility stub for sys/select.h
 * select(), fd_set, FD_SET/CLR/ISSET/ZERO, struct timeval are in winsock2.h.
 */

#ifndef COMPAT_SYS_SELECT_H
#define COMPAT_SYS_SELECT_H

#ifdef _WIN32
#include <winsock2.h>
/* WinSock select() ignores nfds (first arg); otherwise signature matches. */
#else
#include_next <sys/select.h>
#endif

#endif /* COMPAT_SYS_SELECT_H */
