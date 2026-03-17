/*
 * Stub sys/wait.h for Windows/MINGW64.
 * waitpid() is provided by windows-posix.h.
 */

#ifndef COMPAT_SYS_WAIT_H
#define COMPAT_SYS_WAIT_H

#ifdef _WIN32

/* waitpid() options */
#ifndef WNOHANG
#define WNOHANG   1
#endif
#ifndef WUNTRACED
#define WUNTRACED 2
#endif

/* Status inspection macros */
#define WIFEXITED(s)    1
#define WEXITSTATUS(s)  ((s) & 0xFF)
#define WIFSIGNALED(s)  0
#define WTERMSIG(s)     0

#include "../windows-posix.h"

#endif /* _WIN32 */
#endif /* COMPAT_SYS_WAIT_H */
