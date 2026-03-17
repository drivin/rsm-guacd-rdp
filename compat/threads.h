/*
 * Windows/MinGW64 compatibility stub for <threads.h> (C11 threads).
 *
 * WinPR3 (FreeRDP 3.x) unconditionally includes <threads.h>.
 * The mingw-w64 runtime shipped on some CI runners does not yet include it.
 * This stub maps C11 thread types and functions onto POSIX pthreads, which
 * are always available via MinGW64's winpthreads library.
 */

#ifndef COMPAT_THREADS_H
#define COMPAT_THREADS_H

#include <stdlib.h>
#include <time.h>
#include <errno.h>
#include <pthread.h>
#include <sched.h>

#ifdef __cplusplus
extern "C" {
#endif

/* -------------------------------------------------------------------------
 * Types
 * ---------------------------------------------------------------------- */

typedef int   (*thrd_start_t)(void *);
typedef void  (*tss_dtor_t)(void *);

typedef pthread_t       thrd_t;
typedef pthread_mutex_t mtx_t;
typedef pthread_cond_t  cnd_t;
typedef pthread_once_t  once_flag;

typedef struct { pthread_key_t key; } tss_t;

/* -------------------------------------------------------------------------
 * Enumerations
 * ---------------------------------------------------------------------- */

enum {
    thrd_success  = 0,
    thrd_busy     = 1,
    thrd_error    = 2,
    thrd_nomem    = 3,
    thrd_timedout = 4
};

enum {
    mtx_plain     = 0,
    mtx_recursive = 1,
    mtx_timed     = 2
};

/* -------------------------------------------------------------------------
 * Macros
 * ---------------------------------------------------------------------- */

#define ONCE_FLAG_INIT      PTHREAD_ONCE_INIT
#define TSS_DTOR_ITERATIONS 4

/* -------------------------------------------------------------------------
 * Thread functions
 * ---------------------------------------------------------------------- */

typedef struct { thrd_start_t func; void *arg; } _thrd_wrap_t;
static inline void *_thrd_trampoline(void *p) {
    _thrd_wrap_t *w = (_thrd_wrap_t *)p;
    thrd_start_t f = w->func;
    void *a = w->arg;
    free(w);
    return (void *)(intptr_t)f(a);
}

static inline int thrd_create(thrd_t *thr, thrd_start_t func, void *arg) {
    _thrd_wrap_t *w = (_thrd_wrap_t *)malloc(sizeof *w);
    if (!w) return thrd_nomem;
    w->func = func; w->arg = arg;
    if (pthread_create(thr, NULL, _thrd_trampoline, w) != 0) {
        free(w); return thrd_error;
    }
    return thrd_success;
}
static inline int    thrd_join(thrd_t t, int *res) {
    void *ret;
    if (pthread_join(t, &ret) != 0) return thrd_error;
    if (res) *res = (int)(intptr_t)ret;
    return thrd_success;
}
static inline int    thrd_detach(thrd_t t)  { return pthread_detach(t) == 0 ? thrd_success : thrd_error; }
static inline thrd_t thrd_current(void)     { return pthread_self(); }
static inline int    thrd_equal(thrd_t a, thrd_t b) { return pthread_equal(a, b); }
static inline void   thrd_exit(int res)     { pthread_exit((void *)(intptr_t)res); }
static inline void   thrd_yield(void)       { sched_yield(); }
static inline int    thrd_sleep(const struct timespec *ts, struct timespec *rem) {
    return nanosleep(ts, rem);
}

/* -------------------------------------------------------------------------
 * Mutex functions
 * ---------------------------------------------------------------------- */

static inline int mtx_init(mtx_t *m, int type) {
    if (type & mtx_recursive) {
        pthread_mutexattr_t a;
        pthread_mutexattr_init(&a);
        pthread_mutexattr_settype(&a, PTHREAD_MUTEX_RECURSIVE);
        int r = pthread_mutex_init(m, &a) == 0 ? thrd_success : thrd_error;
        pthread_mutexattr_destroy(&a);
        return r;
    }
    return pthread_mutex_init(m, NULL) == 0 ? thrd_success : thrd_error;
}
static inline void mtx_destroy(mtx_t *m)  { pthread_mutex_destroy(m); }
static inline int  mtx_lock(mtx_t *m)     { return pthread_mutex_lock(m)    == 0 ? thrd_success : thrd_error; }
static inline int  mtx_unlock(mtx_t *m)   { return pthread_mutex_unlock(m)  == 0 ? thrd_success : thrd_error; }
static inline int  mtx_trylock(mtx_t *m) {
    int r = pthread_mutex_trylock(m);
    if (r == 0)     return thrd_success;
    if (r == EBUSY) return thrd_busy;
    return thrd_error;
}
static inline int mtx_timedlock(mtx_t *m, const struct timespec *ts) {
    int r = pthread_mutex_timedlock(m, ts);
    if (r == 0)        return thrd_success;
    if (r == ETIMEDOUT) return thrd_timedout;
    return thrd_error;
}

/* -------------------------------------------------------------------------
 * Condition variable functions
 * ---------------------------------------------------------------------- */

static inline int cnd_init(cnd_t *c)              { return pthread_cond_init(c, NULL) == 0 ? thrd_success : thrd_error; }
static inline void cnd_destroy(cnd_t *c)          { pthread_cond_destroy(c); }
static inline int  cnd_signal(cnd_t *c)           { return pthread_cond_signal(c)    == 0 ? thrd_success : thrd_error; }
static inline int  cnd_broadcast(cnd_t *c)        { return pthread_cond_broadcast(c) == 0 ? thrd_success : thrd_error; }
static inline int  cnd_wait(cnd_t *c, mtx_t *m)  { return pthread_cond_wait(c, m)   == 0 ? thrd_success : thrd_error; }
static inline int  cnd_timedwait(cnd_t *c, mtx_t *m, const struct timespec *ts) {
    int r = pthread_cond_timedwait(c, m, ts);
    if (r == 0)        return thrd_success;
    if (r == ETIMEDOUT) return thrd_timedout;
    return thrd_error;
}

/* -------------------------------------------------------------------------
 * Thread-specific storage
 * ---------------------------------------------------------------------- */

static inline int   tss_create(tss_t *k, tss_dtor_t d) { return pthread_key_create(&k->key, d) == 0 ? thrd_success : thrd_error; }
static inline void  tss_delete(tss_t k)                { pthread_key_delete(k.key); }
static inline void *tss_get(tss_t k)                   { return pthread_getspecific(k.key); }
static inline int   tss_set(tss_t k, void *v)          { return pthread_setspecific(k.key, v) == 0 ? thrd_success : thrd_error; }

/* -------------------------------------------------------------------------
 * call_once
 * ---------------------------------------------------------------------- */

static inline void call_once(once_flag *flag, void (*func)(void)) {
    pthread_once(flag, func);
}

#ifdef __cplusplus
}
#endif

#endif /* COMPAT_THREADS_H */
