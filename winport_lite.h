/**
 * Copyright (c) 2015,2018 Wang Xinyong<wang.xy.chn@gmail.com>
 * MIT Licensed.
 */

#ifndef WINPORT_LITE_H_
#define WINPORT_LITE_H_

#if defined(_WIN32)

#ifdef _WIN32_WINNT
# undef _WIN32_WINNT
#endif

/* Enables getaddrinfo() & Co */
#define _WIN32_WINNT 0x0501
#include <ws2tcpip.h>

#include <winsock2.h>
#include <windows.h>

#include <process.h>
#include <errno.h>
#include <time.h>


/*
 * pthread_mutex_*
 */
typedef int pthread_mutexattr_t;
typedef CRITICAL_SECTION pthread_mutex_t;
#define pthread_mutex_init(mutex, attr) InitializeCriticalSection(mutex)
#define pthread_mutex_lock EnterCriticalSection
int pthread_mutex_trylock(pthread_mutex_t *mutex); /* if successful, return 0; otherwise, not 0 */
#define pthread_mutex_unlock LeaveCriticalSection
#define pthread_mutex_destroy DeleteCriticalSection

#define pthread_mutexattr_init(attr) do {} while(0)
#define pthread_mutexattr_settype(attr, ignore) do {} while(0)
#define pthread_mutexattr_destroy(attr) do {} while(0)


/*
 * pthread_once_*
 */
typedef struct {
	volatile unsigned int ran;
	HANDLE event;
} pthread_once_t;
#define PTHREAD_ONCE_INIT {0, NULL}
int pthread_once(pthread_once_t* once, void (*init_routine)(void));


/*
 * pthread_*
 */
typedef HANDLE pthread_t;
typedef int pthread_attr_t;
#define pthread_equal(t1, t2) (t1 == t2)
int pthread_create(pthread_t *thread, const pthread_attr_t* attr,
		void *(*start_routine)(void*), void* args); /* if successful, return 0; otherwise, not 0 */
pthread_t pthread_self();
int pthread_join(pthread_t thread, void **ret);

/*
 * clock_gettime
 */
struct timespec {
	time_t tv_sec;
	long tv_nsec;
};
typedef int clockid_t;
enum {
	CLOCK_REALTIME,
	CLOCK_MONOTONIC
};
int clock_gettime(clockid_t, struct timespec*);

/*
 * pthread_cond_*
 */
typedef struct {
	unsigned int waiters_count;
	CRITICAL_SECTION waiters_count_lock;
	HANDLE signal_event;
	HANDLE broadcast_event;
} pthread_cond_t;

#if !defined(ETIMEDOUT)
#define ETIMEOUT (3447)
#endif

typedef int pthread_condattr_t;
#define pthread_condattr_destroy(cond) do {} while(0)
#define pthread_condattr_init(cond) do {} while(0)

int pthread_cond_init(pthread_cond_t* cond, const pthread_condattr_t* cond_attr);
int pthread_cond_signal(pthread_cond_t* cond);
int pthread_cond_broadcast(pthread_cond_t* cond);
int pthread_cond_wait(pthread_cond_t* cond, pthread_mutex_t* mutex);
/* use monotonic clock */
int pthread_cond_timedwait(pthread_cond_t* cond, pthread_mutex_t* mutex, const struct timespec* abstime);
int pthread_cond_destroy(pthread_cond_t* cond);

/*
 * WINSOCK
 */
#define read(fd, buf, len) recv(fd, (char*)buf, (int)len, 0)
#define write(fd, buf, len) send(fd, (char*)buf, (int)len, 0)
#define close(fd) closesocket(fd)

#if defined(_MSC_VER)
#pragma comment(lib, "ws2_32.lib")
#endif /* _MSC_VER */

#if defined(_MSC_VER)
# define strdup _strdup
#endif

int pipe(int fds[2]);

#if !defined(F_GETFL)
# define F_GETFL 3
#endif

#if !defined(F_SETFL)
# define F_SETFL 4
#endif

#if !defined(O_NONBLOCK)
# define O_NONBLOCK 0400
#endif

/*
 * just simulate for O_NONBLOCK
 * F_GETFL is unsupported
 */
int fcntl(int fd, int op, ...);

#endif /* _WIN32 */

/* socket_int, 0 if sucessful, otherwise non 0 */
#if !defined(_WIN32)
# define socket_init() (0)
#else
int socket_init();
#endif /* _WIN32 */

#endif /* WINPORT_LITE_H_ */

