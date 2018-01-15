/**
 * Copyright (c) 2015,2018 Wang Xinyong<wang.xy.chn@gmail.com>
 * MIT Licensed.
 */

#include <stdlib.h>
#include <stdarg.h>

#include "winport_lite.h"

#if defined(_WIN32)

int pthread_mutex_trylock(pthread_mutex_t *mutex)
{
	if (TryEnterCriticalSection(mutex)) {
		return 0;
	} else {
		return EBUSY;
	}
}

int pthread_once(pthread_once_t* once, void (*init_routine)(void))
{
	HANDLE existing_event, created_event;

	if (once->ran) {
		return 0;
	}

	created_event = CreateEvent(NULL, 1, 0, NULL);
	existing_event = InterlockedCompareExchangePointer(&once->event,
			created_event,
			NULL);

	if (existing_event == NULL) {
		init_routine();

		SetEvent(created_event);
		once->ran = 1;
	} else {
		CloseHandle(created_event);
		WaitForSingleObject(existing_event, INFINITE);
	}
	return 0;
}


static DWORD tls_index;
static pthread_once_t tls_init = PTHREAD_ONCE_INIT;

static void _init_tls(void)
{
	/* FIXME: ignore error */
	tls_index = TlsAlloc();
}

struct _thread_ctx {
	void* (*start_routine)(void*);
	void* arg;
	pthread_t self;
};

static UINT __stdcall _thread_start(void* arg) {
	struct _thread_ctx *ctx_p;
	struct _thread_ctx ctx;

	ctx_p = arg;
	ctx = *ctx_p;
	free(ctx_p);
	TlsSetValue(tls_index, (void*) ctx.self);

	ctx.start_routine(ctx.arg);

	return 0;
}

int pthread_create(pthread_t *tid, const pthread_attr_t* attr, void *(*start_routine)(void*), void* arg)
{
	struct _thread_ctx *ctx;
	HANDLE thread;
	int ret = 0;

	ctx = malloc(sizeof(*ctx));
	if (ctx == NULL) {
		return -1;
	}

	pthread_once(&tls_init, _init_tls);

	ctx->start_routine = start_routine;
	ctx->arg = arg;

	thread = (HANDLE) _beginthreadex(NULL,
			0,
			_thread_start,
			ctx,
			CREATE_SUSPENDED,
			NULL);

	if (thread == NULL) {
		free(ctx);
		ret = -1;
	} else {
		*tid = thread;
		ctx->self = thread;
		ResumeThread(thread);
	}

	return ret;
}

pthread_t pthread_self() {
	return (pthread_t)TlsGetValue(tls_index);
}

int pthread_join(pthread_t tid, void **ret)
{
	if (WaitForSingleObject(tid, INFINITE)) {
		return (int)GetLastError();
	} else {
		CloseHandle(tid);
		if (ret != NULL) {
			*ret = NULL;
		}
		MemoryBarrier();
	}
	return 0;
}

static LARGE_INTEGER counts_per_sec;
static pthread_once_t _counts_per_once = PTHREAD_ONCE_INIT;

static void _init_counts_per_sec(void)
{
	QueryPerformanceFrequency(&counts_per_sec);
}

static const unsigned __int64 epoch = 116444736000000000ULL;

int clock_gettime(clockid_t c, struct timespec* ts)
{
	pthread_once(&_counts_per_once, _init_counts_per_sec);

	if (ts == NULL) {
		return -1;
	}

	if (counts_per_sec.QuadPart <= 0) {
		return -1;
	}

	if (c == CLOCK_REALTIME) {
		FILETIME    file_time;
		SYSTEMTIME  system_time;
		ULARGE_INTEGER ularge;

		GetSystemTime(&system_time);
		SystemTimeToFileTime(&system_time, &file_time);
		ularge.LowPart = file_time.dwLowDateTime;
		ularge.HighPart = file_time.dwHighDateTime;

		ts->tv_sec = (time_t) ((ularge.QuadPart - epoch) / 10000000L);
		ts->tv_nsec = (long) (system_time.wMilliseconds * 1000000);
		return 0;
	} else if (c == CLOCK_MONOTONIC) {
		LARGE_INTEGER count;
		if (QueryPerformanceCounter(&count) == 0) {
			return -1;
		}

		ts->tv_sec = (time_t)(count.QuadPart / counts_per_sec.QuadPart);
		ts->tv_nsec = (long)(((count.QuadPart % counts_per_sec.QuadPart) * 1e9) / counts_per_sec.QuadPart);
		return 0;
	}
	return -1;
}

int pthread_cond_init(pthread_cond_t* cond, const pthread_condattr_t* attr)
{
	(void)attr;

	/* Initialize the count to 0. */
	cond->waiters_count = 0;

	InitializeCriticalSection(&cond->waiters_count_lock);

	/* Create an auto-reset event. */
	cond->signal_event = CreateEvent(NULL,  /* no security */
			FALSE, /* auto-reset event */
			FALSE, /* non-signaled initially */
			NULL); /* unnamed */
	if (!cond->signal_event) {
		goto error2;
	}

	/* Create a manual-reset event. */
	cond->broadcast_event = CreateEvent(NULL,  /* no security */
			TRUE,  /* manual-reset */
			FALSE, /* non-signaled */
			NULL); /* unnamed */
	if (!cond->broadcast_event) {
		goto error;
	}

	return 0;

error:
	CloseHandle(cond->signal_event);
error2:
	DeleteCriticalSection(&cond->waiters_count_lock);
	return -1;
}

int pthread_cond_signal(pthread_cond_t* cond)
{
	int have_waiters;

	/* Avoid race conditions. */
	EnterCriticalSection(&cond->waiters_count_lock);
	have_waiters = cond->waiters_count > 0;
	LeaveCriticalSection(&cond->waiters_count_lock);

	if (have_waiters) {
		SetEvent(cond->signal_event);
	}

	return 0;
}

int pthread_cond_broadcast(pthread_cond_t* cond)
{
	int have_waiters;

	/* Avoid race conditions. */
	EnterCriticalSection(&cond->waiters_count_lock);
	have_waiters = cond->waiters_count > 0;
	LeaveCriticalSection(&cond->waiters_count_lock);

	if (have_waiters) {
		SetEvent(cond->broadcast_event);
	}

	return 0;
}

static int _cond_wait_helper(pthread_cond_t* cond,
		pthread_mutex_t* mutex, DWORD dwMilliseconds) {
	DWORD result;
	int last_waiter;
	HANDLE handles[2] = {
		cond->signal_event,
		cond->broadcast_event
	};

	EnterCriticalSection(&cond->waiters_count_lock);
	cond->waiters_count++;
	LeaveCriticalSection(&cond->waiters_count_lock);

	pthread_mutex_unlock(mutex);

	result = WaitForMultipleObjects(2, handles, FALSE, dwMilliseconds);

	EnterCriticalSection(&cond->waiters_count_lock);
	cond->waiters_count--;
	last_waiter = ((result == WAIT_OBJECT_0 + 1)
			&& (cond->waiters_count == 0));
	LeaveCriticalSection(&cond->waiters_count_lock);

	if (last_waiter) {
		/* We're the last waiter to be notified or to stop waiting, so reset the */
		/* the manual-reset event. */
		ResetEvent(cond->broadcast_event);
	}

	/* Reacquire the <mutex>. */
	pthread_mutex_lock(mutex);

	if (result == WAIT_OBJECT_0 || result == WAIT_OBJECT_0 + 1)
		return 0;

	if (result == WAIT_TIMEOUT)
		return ETIMEDOUT;

	abort();
	return -1;
}

int pthread_cond_wait(pthread_cond_t* cond, pthread_mutex_t* mutex)
{
	return _cond_wait_helper(cond, mutex, INFINITE);
}

int pthread_cond_timedwait(pthread_cond_t* cond, pthread_mutex_t* mutex, const struct timespec* abstime)
{
	struct timespec now;
	DWORD dw_timeout;

	if (cond == NULL || mutex == NULL) {
		return -1;
	}
	if (abstime) {
		if (clock_gettime(CLOCK_REALTIME, &now) != 0) {
			return -1;
		} else {
			long timeout;
			timeout = (long)((abstime->tv_sec - now.tv_sec) * 1000);
			timeout += (long)((abstime->tv_nsec - now.tv_nsec) / 100000);
			dw_timeout = (DWORD)timeout;
		}
	} else {
		dw_timeout = INFINITE;
	}

	return _cond_wait_helper(cond, mutex, dw_timeout);
}

int pthread_cond_destroy(pthread_cond_t* cond)
{
	CloseHandle(cond->signal_event);
	CloseHandle(cond->broadcast_event);

	DeleteCriticalSection(&cond->waiters_count_lock);
	return 0;
}

int pipe(int fds[2])
{
	static char yes = 1;

	struct sockaddr_storage ss;
	struct sockaddr_in *sa = (struct sockaddr_in *)&ss;
	socklen_t ss_len;
	SOCKET rd, wt;
	SOCKET lstn;

	fds[0] = -1;
	fds[1] = -1;

	memset(&ss, 0, sizeof(ss));
	sa->sin_family = AF_INET;
	sa->sin_addr.s_addr = htonl(INADDR_LOOPBACK);
	sa->sin_port = 0;
	ss_len = sizeof(struct sockaddr_in);

	lstn = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (lstn == -1){
		goto exit;
	}
	if (setsockopt(lstn, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes)) != 0) {
		goto err1;
	}
	if (bind(lstn, (struct sockaddr *)&ss, ss_len) == -1){
		goto err1;
	}

	if (listen(lstn, 1) == -1){
		goto err1;
	}

	memset(&ss, 0, sizeof(ss));
	ss_len = sizeof(ss);
	if (getsockname(lstn, (struct sockaddr *)&ss, &ss_len) < 0) {
		goto err1;
	}

	sa->sin_family = AF_INET;
	sa->sin_addr.s_addr = htonl(INADDR_LOOPBACK);
	ss_len = sizeof(struct sockaddr_in);

	rd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (rd == -1) {
		goto err1;
	}

	if (connect(rd, (struct sockaddr *)&ss, ss_len) < 0){
		goto err2;
	}
	wt = accept(lstn, NULL, 0);
	if (wt == INVALID_SOCKET){
		goto err2;
	}

	close(lstn);

	fds[0] = (int)rd;
	fds[1] = (int)wt;
	return 0;

err2:
	close(rd);
err1:
	close(lstn);
exit:
	return -1;
}

int fcntl(int fd, int op, ...)
{
	va_list va;
	int arg;

	va_start(va, op);
	arg = va_arg(va, int);
	va_end(va);

	if (op == F_GETFL) {
		return 0;
	} else if (op == F_SETFL) {
		u_long nb;
		if ((arg & O_NONBLOCK) != 0) {
			nb = 1;
		} else {
			nb = 0;
		}

		if (ioctlsocket(fd, FIONBIO, &nb) == 0) {
			return 0;
		}
	}

	return -1;
}

int socket_init()
{
	WSADATA wsaData;

	if (WSAStartup(MAKEWORD(2,0), &wsaData) != 0) {
		return -1;
	}
	return 0;
}

#endif /* _WIN32 */

