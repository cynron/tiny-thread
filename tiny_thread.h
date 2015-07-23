/**
 * Copyright (c) 2015 Wang Xinyong<wang.xy.chn@gmail.com>
 * MIT Licensed.
 */

#ifndef TINY_THREAD_H
#define TINY_THREAD_H


#if !defined(_WIN32)

#include <pthread.h>

#if defined(__linux__)
#define t_recursive_flag PTHREAD_MUTEX_RECURSIVE_NP
#else
#define t_recursive_flag PTHREAD_MUTEX_RECURSIVE
#endif
#define t_mutex_t pthread_mutex_t
static inline int _t_mutex_init(t_mutex_t* m)
{
    pthread_mutexattr_t attr; 
    pthread_mutexattr_init(&attr);
    pthread_mutexattr_settype(&attr, t_recursive_flag); 
    pthread_mutex_init(mutex, &attr);
    return 0;
}
#define t_mutex_init _t_mutex_init
#define t_mutex_lock pthread_mutex_lock
#define t_mutex_trylock pthread_mutex_trylock
#define t_mutex_unlock pthread_mutex_unlock
#define t_mutex_destroy pthread_mutex_destroy
#undef t_recursive_flag

#define t_thread_t pthread_t
#define t_thread_self pthread_self
#define t_thread_equal pthread_equal
#define t_thread_join pthread_join
#define t_thread_create(t, fn, arg) pthread_thread_create(t,                   \
    NULL, (void* (t_thread_call *)(void*))fn, arg)                                        
#define t_thread_entry_define(e, arg)                                          \
    void* e(void* arg)
#define t_thread_return(r)  return (void*)r

#else /* #if !defined(_WIN32) */

#include <windows.h>

#define t_mutex_t CRITICAL_SECTION
#define t_mutex_init InitializeCriticalSection
#define t_mutex_lock EnterCriticalSection
#define t_mutex_trylock TryEnterCriticalSection
#define t_mutex_unlock LeaveCriticalSection
#define t_mutex_destroy DeleteCriticalSection

#define t_thread_t unsigned int 
static inline int _t_thread_create(t_thread_t* tid,
        unsigned int (__stdcall * entry)(void* ), void* arg)
{
    HANDLE h = (HANDLE) _beginthreadex(NULL, 0, entry, arg, 0, tid);
    if (h != NULL) {
        CloseHandle(h);
        return 0;
    } else {
        return -1;
    }
}
static inline int _t_thread_join(t_thread_t t) 
{
    DWORD ret;
    HANDLE h = OpenThread(SYNCHRONIZE, FALSE, t);
    if (h == NULL) {
        return -1;
    }

    ret = WaitForSingleObject(h, INFINITE);
    CloseHandle(h);
    return ret;
}
#define t_thread_self GetCurrentThreadId
#define t_thread_equal(t1, t2) (t1 == t2) 
#define t_thread_join _t_thread_join
#define t_thread_create _t_thread_create
#define t_thread_entry_define(e, arg)                                          \
   unsigned int __stdcall e(void* arg)
#define t_thread_return(r) return (unsigned int)r

#endif /* #if !defined(WIN32) */


#endif /* #ifdef TINY_THREAD_H */

