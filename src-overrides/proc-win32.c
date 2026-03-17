/*
 * Windows replacement for proc.c
 *
 * On Linux/POSIX, guacd forks a child process per connection and uses a
 * UNIX-domain socketpair to pass user file descriptors from the daemon to
 * the child. On Windows neither fork() nor AF_UNIX socketpair() are
 * available in MINGW64.
 *
 * This replacement keeps the same public API (guacd_create_proc /
 * guacd_proc_stop) but implements it with pthreads:
 *
 *   - guacd_create_proc() opens a _pipe() pair and spawns a detached
 *     pthread that runs the protocol handler.
 *
 *   - The "parent" side of the proc keeps proc->fd_socket = write end of
 *     the pipe.  connection.c sends user-socket fds down this pipe via
 *     guacd_send_fd() (implemented in move-fd-win32.c as a plain write).
 *
 *   - The proc thread reads fd integers from the read end of the pipe
 *     (guacd_recv_fd) and spawns per-user threads exactly as the original
 *     proc.c does.
 *
 *   - Closing proc->fd_socket (write end) signals EOF to the proc thread,
 *     causing it to exit cleanly.
 *
 * proc->pid is set to the host process PID in the "parent" role so that
 * the existing pid != 0 check in guacd_proc_stop() still works.
 *
 * A pthread_t field is added to guacd_proc on Windows (see proc-win32.h)
 * so that connection.c can pthread_join() instead of waitpid().
 */

#include "config.h"

#include "log.h"
#include "move-fd.h"
#include "proc.h"
#include "proc-map.h"

#include <guacamole/client.h>
#include <guacamole/error.h>
#include <guacamole/mem.h>
#include <guacamole/parser.h>
#include <guacamole/plugin.h>
#include <guacamole/protocol.h>
#include <guacamole/socket.h>
#include <guacamole/user.h>

#include <errno.h>
#include <fcntl.h>
#include <io.h>        /* _pipe() */
#include <pthread.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <unistd.h>

/* -----------------------------------------------------------------------
 * Per-user thread
 * --------------------------------------------------------------------- */

typedef struct guacd_user_thread_params {
    guacd_proc* proc;
    int fd;
    int owner;
} guacd_user_thread_params;

static void* guacd_user_thread(void* data) {

    guacd_user_thread_params* params = (guacd_user_thread_params*) data;
    guacd_proc* proc = params->proc;
    guac_client* client = proc->client;

    /* Open a guac_socket from the user's fd */
    guac_socket* socket = guac_socket_open(params->fd);
    if (socket == NULL) {
        guac_mem_free(params);
        return NULL;
    }

    /* Build skeleton user */
    guac_user* user = guac_user_alloc();
    user->socket = socket;
    user->client = client;
    user->owner  = params->owner;

    /* Handle connection from handshake until disconnect */
    guac_user_handle_connection(user, GUACD_USEC_TIMEOUT);

    /* Stop client if this was the last user */
    if (client->connected_users == 0) {
        guacd_log(GUAC_LOG_INFO,
                "Last user of connection \"%s\" disconnected",
                client->connection_id);
        guacd_proc_stop(proc);
    }

    guac_socket_free(socket);
    guac_user_free(user);
    guac_mem_free(params);
    return NULL;

}

static void guacd_proc_add_user(guacd_proc* proc, int fd, int owner) {

    guacd_user_thread_params* params =
        guac_mem_alloc(sizeof(guacd_user_thread_params));
    params->proc  = proc;
    params->fd    = fd;
    params->owner = owner;

    pthread_t user_thread;
    pthread_create(&user_thread, NULL, guacd_user_thread, params);
    pthread_detach(user_thread);

}

/* -----------------------------------------------------------------------
 * Timed client free (identical to original proc.c)
 * --------------------------------------------------------------------- */

typedef struct guacd_client_free {
    guac_client*    client;
    pthread_cond_t  completed_cond;
    pthread_mutex_t completed_mutex;
    int             completed;
} guacd_client_free;

static void* guacd_client_free_thread(void* data) {

    guacd_client_free* op = (guacd_client_free*) data;
    guac_client_free(op->client);

    pthread_mutex_lock(&op->completed_mutex);
    op->completed = 1;
    pthread_cond_broadcast(&op->completed_cond);
    pthread_mutex_unlock(&op->completed_mutex);
    return NULL;

}

static int guacd_timed_client_free(guac_client* client, int timeout) {

    pthread_t free_thread;

    guacd_client_free op = {
        .client         = client,
        .completed_cond = PTHREAD_COND_INITIALIZER,
        .completed_mutex= PTHREAD_MUTEX_INITIALIZER,
        .completed      = 0
    };

    struct timeval tv;
    if (gettimeofday(&tv, NULL)) return 1;

    struct timespec deadline = {
        .tv_sec  = tv.tv_sec + timeout,
        .tv_nsec = tv.tv_usec * 1000
    };

    if (pthread_mutex_lock(&op.completed_mutex)) return 1;

    if (!pthread_create(&free_thread, NULL, guacd_client_free_thread, &op))
        (void) pthread_cond_timedwait(&op.completed_cond,
                                      &op.completed_mutex, &deadline);

    (void) pthread_mutex_unlock(&op.completed_mutex);
    return !op.completed;

}

/* -----------------------------------------------------------------------
 * Proc thread
 * --------------------------------------------------------------------- */

typedef struct guacd_proc_thread_data {
    guacd_proc* proc;
    char*       protocol;   /* strdup'd copy; freed by thread */
    int         read_fd;    /* read end of _pipe() */
} guacd_proc_thread_data;

static void* guacd_proc_thread_main(void* data) {

    guacd_proc_thread_data* td = (guacd_proc_thread_data*) data;
    guacd_proc* proc      = td->proc;
    char*       protocol  = td->protocol;
    int         read_fd   = td->read_fd;
    guac_mem_free(td);

    guac_client* client = proc->client;

    /* Load the protocol plugin */
    if (guac_client_load_plugin(client, protocol)) {

        if (guac_error == GUAC_STATUS_NOT_FOUND)
            guacd_log(GUAC_LOG_WARNING,
                    "Support for protocol \"%s\" is not installed", protocol);
        else
            guacd_log_guac_error(GUAC_LOG_ERROR,
                    "Unable to load client plugin");

        guac_mem_free(protocol);
        goto cleanup_client;
    }

    guac_mem_free(protocol);

    int owner = 1;

    /* NOTE: guac_socket_require_keep_alive() is NOT called here.
     * client->socket is NULL until the first user joins; keep-alive is
     * handled per-user inside guac_user_handle_connection(). */

    /* Receive user fds from the daemon connection thread */
    int received_fd;
    while ((received_fd = guacd_recv_fd(read_fd)) != -1) {
        guacd_proc_add_user(proc, received_fd, owner);
        owner = 0;
    }

cleanup_client:

    guac_client_stop(client);

    guacd_log(GUAC_LOG_DEBUG, "Requesting termination of client...");
    if (guacd_timed_client_free(client, GUACD_CLIENT_FREE_TIMEOUT))
        guacd_log(GUAC_LOG_WARNING,
                "Client did not terminate in a timely manner.");
    else
        guacd_log(GUAC_LOG_DEBUG, "Client terminated successfully.");

    close(read_fd);

    /*
     * NOTE: proc itself is freed by connection.c after pthread_join().
     * Do NOT free proc here to avoid a use-after-free race.
     */

    return NULL;

}

/* -----------------------------------------------------------------------
 * Public API
 * --------------------------------------------------------------------- */

guacd_proc* guacd_create_proc(const char* protocol) {

    int pipefd[2];

    /* Create a unidirectional pipe:
     *   pipefd[0] = read end  (proc thread reads user fds from here)
     *   pipefd[1] = write end (connection thread writes user fds here)
     */
    if (_pipe(pipefd, 4096, _O_BINARY) < 0) {
        guacd_log(GUAC_LOG_ERROR, "Error creating pipe: %s", strerror(errno));
        return NULL;
    }

    /* Allocate proc structure */
    guacd_proc* proc = guac_mem_zalloc(sizeof(guacd_proc));
    if (proc == NULL) {
        close(pipefd[0]);
        close(pipefd[1]);
        return NULL;
    }

    proc->client = guac_client_alloc();
    if (proc->client == NULL) {
        guacd_log_guac_error(GUAC_LOG_ERROR, "Unable to create client");
        close(pipefd[0]);
        close(pipefd[1]);
        guac_mem_free(proc);
        return NULL;
    }

    proc->client->log_handler = guacd_client_log;

    /*
     * proc->fd_socket = write end of pipe.
     * connection.c uses this to send user fds via guacd_send_fd().
     *
     * proc->pid is set non-zero so that guacd_proc_stop() takes the
     * "parent" code path (close write end + stop client).
     */
    proc->fd_socket = pipefd[1];
    proc->pid       = (pid_t) GetCurrentProcessId();

    /* Set up thread arguments */
    guacd_proc_thread_data* td = guac_mem_alloc(sizeof(guacd_proc_thread_data));
    td->proc     = proc;
    td->protocol = strdup(protocol);
    td->read_fd  = pipefd[0];

    /* Spawn proc thread */
    if (pthread_create(&proc->thread, NULL, guacd_proc_thread_main, td) != 0) {
        guacd_log(GUAC_LOG_ERROR, "Cannot create proc thread: %s",
                strerror(errno));
        guac_mem_free(td->protocol);
        guac_mem_free(td);
        close(pipefd[0]);
        close(pipefd[1]);
        guac_client_free(proc->client);
        guac_mem_free(proc);
        return NULL;
    }

    return proc;

}

void guacd_proc_stop(guacd_proc* proc) {

    /*
     * "Parent" role: close the write end of the pipe.
     * This sends EOF to the proc thread's guacd_recv_fd() loop, causing
     * it to exit and clean up.
     */
    if (proc->pid != 0) {
        int fd = proc->fd_socket;
        if (fd >= 0) {
            proc->fd_socket = -1;
            close(fd);
        }
    }

    /* Always signal the client to stop (safe to call from any thread) */
    guac_client_stop(proc->client);

}
