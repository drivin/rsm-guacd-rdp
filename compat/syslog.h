/*
 * Windows compatibility stub for syslog.h
 * Routes syslog calls to stderr output.
 */

#ifndef COMPAT_SYSLOG_H
#define COMPAT_SYSLOG_H

#include <stdarg.h>
#include <stdio.h>

/* Log priority constants */
#define LOG_EMERG   0
#define LOG_ALERT   1
#define LOG_CRIT    2
#define LOG_ERR     3
#define LOG_WARNING 4
#define LOG_NOTICE  5
#define LOG_INFO    6
#define LOG_DEBUG   7

/* openlog() option flags (ignored on Windows) */
#define LOG_PID     0x01
#define LOG_CONS    0x02
#define LOG_NDELAY  0x08
#define LOG_NOWAIT  0x10
#define LOG_PERROR  0x20

/* Facility codes (ignored on Windows) */
#define LOG_KERN    (0<<3)
#define LOG_USER    (1<<3)
#define LOG_MAIL    (2<<3)
#define LOG_DAEMON  (3<<3)
#define LOG_AUTH    (4<<3)
#define LOG_SYSLOG  (5<<3)
#define LOG_LPR     (6<<3)
#define LOG_NEWS    (7<<3)
#define LOG_LOCAL0  (16<<3)

static inline void openlog(const char* ident, int option, int facility) {
    /* No-op: syslog not available on Windows */
    (void)ident; (void)option; (void)facility;
}

static inline void closelog(void) {
    /* No-op */
}

static inline void syslog(int priority, const char* fmt, ...) {
    /* Route to stderr */
    va_list args;
    va_start(args, fmt);
    vfprintf(stderr, fmt, args);
    va_end(args);
    fprintf(stderr, "\n");
    (void)priority;
}

#endif /* COMPAT_SYSLOG_H */
