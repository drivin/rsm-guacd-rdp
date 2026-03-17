/*
 * Windows-extended replacement for proc.h
 *
 * Identical to the upstream proc.h except that on Windows (_WIN32) a
 * pthread_t thread field is added to guacd_proc so that connection.c can
 * pthread_join() on the proc thread instead of waitpid() on a child PID.
 */

#ifndef GUACD_PROC_H
#define GUACD_PROC_H

#include "config.h"

#include <guacamole/client.h>
#include <guacamole/parser.h>

#ifdef _WIN32
#include <pthread.h>
#include <windows.h>
#else
#include <unistd.h>
#endif

/**
 * The number of milliseconds to wait for messages in any phase before
 * timing out and closing the connection with an error.
 */
#define GUACD_TIMEOUT 15000

/**
 * The number of microseconds to wait for messages in any phase before
 * timing out and closing the connection with an error. This is always
 * equal to GUACD_TIMEOUT * 1000.
 */
#define GUACD_USEC_TIMEOUT (GUACD_TIMEOUT*1000)

/**
 * The number of seconds to wait for any particular guac_client instance
 * to be freed following disconnect.
 */
#define GUACD_CLIENT_FREE_TIMEOUT 5

/**
 * Process information of the internal remote desktop client.
 */
typedef struct guacd_proc {

    /**
     * The process ID of the client process (POSIX), or the host PID cast
     * to pid_t (Windows - used only as a non-zero sentinel).
     */
    pid_t pid;

    /**
     * Write end of the pipe used to send user fds to the proc thread
     * (Windows), or the UNIX domain socket fd for the same purpose
     * (POSIX).
     */
    int fd_socket;

    /**
     * The client instance. Fully initialised only in the proc thread;
     * the connection thread has a skeleton client (connection_id + log
     * handler only).
     */
    guac_client* client;

#ifdef _WIN32
    /**
     * Handle for the proc pthread. Used by connection.c to pthread_join()
     * instead of waitpid() after the proc thread finishes.
     */
    pthread_t thread;
#endif

} guacd_proc;

/**
 * Creates a new background process/thread for handling the given protocol.
 *
 * @param protocol
 *     The Guacamole protocol name (e.g. "rdp").
 *
 * @return
 *     A newly-allocated guacd_proc, or NULL on failure.
 */
guacd_proc* guacd_create_proc(const char* protocol);

/**
 * Signals the given process/thread to stop accepting new users and clean up.
 *
 * @param proc
 *     The process to stop.
 */
void guacd_proc_stop(guacd_proc* proc);

#endif /* GUACD_PROC_H */
