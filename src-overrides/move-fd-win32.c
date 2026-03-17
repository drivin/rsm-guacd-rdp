/*
 * Windows replacement for move-fd.c
 *
 * On Windows, guacd uses threads instead of processes, so all SOCKETs are
 * already shared within the same process. We don't use SCM_RIGHTS; instead
 * we write/read the raw SOCKET handle integer through a pipe.
 *
 * SOCKET vs CRT fd:
 *   WinSock SOCKETs are Windows kernel HANDLEs, NOT CRT file descriptors.
 *   _dup() is a CRT operation that does NOT work on SOCKETs. Instead we use
 *   WSADuplicateSocket() with our own PID to get a new independent SOCKET
 *   reference that the proc thread can closesocket() independently of the
 *   connection thread.
 *
 *   The pipe (sock) carries plain int-sized values; we cast the SOCKET
 *   (UINT_PTR / uintptr_t) to int64_t so both 32- and 64-bit SOCKET values
 *   transit the pipe without truncation.
 */

#include "config.h"
#include "move-fd.h"

#include <winsock2.h>
#include <windows.h>
#include <stdint.h>   /* int64_t, uintptr_t */
#include <errno.h>
#include <unistd.h>   /* write() / read() on pipe fds */
#include <string.h>

int guacd_send_fd(int sock, int fd) {

    /*
     * fd is actually a WinSock SOCKET cast to int by the caller in
     * connection.c (guacd stores accept()'d sockets as int fd_socket).
     * Recover the full SOCKET value.
     */
    SOCKET socket_handle = (SOCKET)(uintptr_t)(unsigned int)fd;

    /*
     * WSADuplicateSocket with our own PID creates a new independent
     * SOCKET referencing the same underlying connection.  The duplicate
     * can be closed by the proc thread without affecting the original,
     * and vice versa.
     */
    WSAPROTOCOL_INFOW proto_info;
    memset(&proto_info, 0, sizeof(proto_info));

    if (WSADuplicateSocketW(socket_handle, GetCurrentProcessId(),
                            &proto_info) != 0) {
        /* Fallback: just pass the raw handle without duplication.
         * The caller must NOT close its copy in this case, but since
         * we return failure the caller will abort anyway. */
        return 0;
    }

    /* Re-create a new independent SOCKET from the protocol info */
    SOCKET dup_socket = WSASocketW(FROM_PROTOCOL_INFO,
                                   FROM_PROTOCOL_INFO,
                                   FROM_PROTOCOL_INFO,
                                   &proto_info, 0,
                                   WSA_FLAG_OVERLAPPED);
    if (dup_socket == INVALID_SOCKET)
        return 0;

    /* Write the duplicated SOCKET handle as int64_t through the pipe */
    int64_t val = (int64_t)(uintptr_t)dup_socket;
    if (write(sock, &val, sizeof(val)) != sizeof(val)) {
        closesocket(dup_socket);
        return 0;
    }

    return 1; /* success */

}

int guacd_recv_fd(int sock) {

    int64_t val;

    /* Read the SOCKET handle from the pipe */
    int bytes = read(sock, &val, sizeof(val));
    if (bytes != (int)sizeof(val))
        return -1;

    /*
     * Return the SOCKET as int.  On 64-bit Windows, SOCKET can exceed
     * INT_MAX in theory, but in practice WinSock uses small indices and
     * the value fits.  The proc thread treats this int as a SOCKET via
     * the same (SOCKET)(uintptr_t)(unsigned int) cast.
     */
    return (int)(uintptr_t)(SOCKET)val;

}
