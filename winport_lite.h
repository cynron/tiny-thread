/**
 * Copyright (c) 2015 Wang Xinyong<wang.xy.chn@gmail.com>
 * MIT Licensed.
 */

#ifndef WIN_COMPAT_H_
#define WIN_COMPAT_H_ 

#if defined(_WIN32) || defined(WIN32)

#include <windows.h>
#include <process.h>

#if defined(_MSC_VER)
#define inline __inline
#endif

/*
 * pthread_mutex_* 
 */
typedef pthread_mutex_t CRITICAL_SECTION
#define pthread_mutex_init InitializeCriticalSection
#define pthread_mutex_lock EnterCriticalSection
int pthread_mutex_trylock(pthread_mutex_t *mutex); /* if successful, return 0; otherwise, not 0 */ 
#define pthread_mutex_unlock LeaveCriticalSection
#define pthread_mutex_destroy DeleteCriticalSection


/*
 * pthread_once_*
 */
typedef struct {
    volatile unsigned int ran;
    HANDLE event;
} pthread_once_t;
#define PHTREAD_ONCE_INIT {0, NULL}
int pthread_once(pthread_once_t* once, void (*init_routine)(void));


/*
 * pthread_*
 */
typedef HANDLE pthread_t
#define pthread_equal(t1, t2) (t1 == t2)
int pthread_create(pthread_t *thread, const void *attr,
        void *(*start_routine)(void*), void* args); /* if successful, return 0; otherwise, not 0 */ 
pthread_t pthread_self();
int pthread_join(pthread_t thread, void **ret);

/*
 * pthread_cond_*
 */
typedef struct {
    unsigned int waiters_count;
    CRITICAL_SECTION waiters_count_lock;
    HANDLE signal_event;
    HANDLE broadcast_event;
} pthread_cond_t;
int pthread_cond_init(pthread_cond_t* cond, void* cond_attr);
int pthread_cond_signal(pthread_cond_t* cond);
int pthread_cond_broadcast(pthread_cond_t* cond);
int pthread_cond_wait(pthread_cond_t* cond, pthread_mutex_t* mutex); 

struct timespec {
};

int pthread_cond_timedwait(pthread_cond_t* cond, pthread_mutex* mutex, const struct timespec* abstime);
int pthread_cond_destroy(pthread_cond_t* cond);

/*
 * WINSOCK
 */


#endif

