/*
 * Windows compatibility stub for <pwd.h>
 *
 * Also provides related POSIX user/FS stubs used by guacamole's RDP client.c:
 *   getpwuid / getpwnam / getuid / getpwnam_r / getpwuid_r
 *   faccessat / AT_FDCWD / W_OK / R_OK / F_OK
 *   setenv
 */

#ifndef COMPAT_PWD_H
#define COMPAT_PWD_H

#ifdef _WIN32

#include <sys/types.h>
#include <stddef.h>
#include <io.h>       /* _access */
#include <stdlib.h>   /* _putenv_s */
#include <windows.h>  /* SetEnvironmentVariableA */
#include <errno.h>

/* -----------------------------------------------------------------------
 * struct passwd
 * --------------------------------------------------------------------- */

struct passwd {
    char *pw_name;   /* username       */
    char *pw_passwd; /* password       */
    int   pw_uid;    /* user ID        */
    int   pw_gid;    /* group ID       */
    char *pw_gecos;  /* user info      */
    char *pw_dir;    /* home directory */
    char *pw_shell;  /* login shell    */
};

/* -----------------------------------------------------------------------
 * User ID stubs — Windows has no real uid; return 0 (treat as root)
 * --------------------------------------------------------------------- */

static inline unsigned int getuid(void)  { return 0; }
static inline unsigned int geteuid(void) { return 0; }
static inline unsigned int getgid(void)  { return 0; }
static inline unsigned int getegid(void) { return 0; }

/* -----------------------------------------------------------------------
 * passwd lookup stubs — always "not found"
 * --------------------------------------------------------------------- */

static inline struct passwd *getpwnam(const char *name) {
    (void)name; return NULL;
}
static inline struct passwd *getpwuid(unsigned int uid) {
    (void)uid; return NULL;
}
static inline int getpwnam_r(const char *name, struct passwd *pwd,
                              char *buf, size_t buflen,
                              struct passwd **result) {
    (void)name; (void)pwd; (void)buf; (void)buflen;
    if (result) *result = NULL;
    return 0;  /* 0 + result=NULL → not found */
}
static inline int getpwuid_r(unsigned int uid, struct passwd *pwd,
                              char *buf, size_t buflen,
                              struct passwd **result) {
    (void)uid; (void)pwd; (void)buf; (void)buflen;
    if (result) *result = NULL;
    return 0;
}

/* -----------------------------------------------------------------------
 * faccessat / AT_FDCWD / W_OK / R_OK / F_OK
 *
 * faccessat(AT_FDCWD, path, mode, 0) == access(path, mode).
 * On Windows, _access() uses the same mode bits.
 * --------------------------------------------------------------------- */

#ifndef AT_FDCWD
#define AT_FDCWD  (-100)
#endif
#ifndef F_OK
#define F_OK 0
#endif
#ifndef R_OK
#define R_OK 4
#endif
#ifndef W_OK
#define W_OK 2
#endif
#ifndef X_OK
#define X_OK 1   /* Windows has no exec bit; _access ignores it */
#endif

static inline int faccessat(int dirfd, const char *path, int mode, int flags) {
    (void)dirfd; (void)flags;
    /* Only AT_FDCWD (relative to cwd) is needed by guacamole */
    return _access(path, mode);
}

#endif /* _WIN32 */
#endif /* COMPAT_PWD_H */
