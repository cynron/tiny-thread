#include "win_compat.h"
#include <stdlib.h>

inline int pthread_mutex_trylock(pthread_mutex_t *mutex)
{
    if (TryEnterCriticalSection(mutex)) {
        return 0;
    } else {
        return EBUSY;
    }
}

int pthread_once(pthread_once_t* once, void (*init_routine)(void))
{
    DWORD result;
    HANDLE existing_event, created_event;

    if (once->ran) {
	return;
    }

    created_event = CreateEvent(NULL, 1, 0, NULL);
    existing_event = InterlockedCompareExchangePointer(&once->event,
                                                     created_event,
                                                     NULL);

    if (existing_event == NULL) {
	    init_routine();

	    SetEvent(created_event);
	    guard->ran = 1;

    } else {
	    CloseHandle(created_event);
	    WaitForSingleObject(existing_event, INFINITE);
    }
}


static DWORD tls_index;
static pthread_once_t tls_init = PTHREAD_ONCE_INIT;

static void _init_tls() 
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

	pthread_once(&tls_init, _init_tls);

	TlsSetValue(tls_index, (void*) ctx.self);
	ctx.start_routine(ctx.arg);

	return 0;
}



inline int pthread_create(pthread_t *thread, const void* attr, void *(*start_routine)(void*), void* args)
{
        struct _thread_ctx *ctx;
	HANDLE h;

	ctx = malloc(sizeof(struct _thread_ctx));
	if (ctx == NULL) {
		return -1;
	}
	struct thread_ctx* ctx;
  int err;
  HANDLE thread;

  ctx = uv__malloc(sizeof(*ctx));
  if (ctx == NULL)
    return UV_ENOMEM;

  ctx->entry = entry;
  ctx->arg = arg;

  thread = (HANDLE) _beginthreadex(NULL,
                                   0,
                                   _thread_start,
                                   ctx,
                                   CREATE_SUSPENDED,
                                   NULL);
  if (thread == NULL) {
    err = errno;
    free(ctx);
  } else {
    err = 0;
    *tid = thread;
    ctx->self = thread;
    ResumeThread(thread);
  }

  return err;
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
		*ret = NULL;
		MemoryBarrier();
	}
	return 0;
}



static int uv_cond_wait_helper(uv_cond_t* cond, uv_mutex_t* mutex,
    DWORD dwMilliseconds) {
  DWORD result;
  int last_waiter;
  HANDLE handles[2] = {
    cond->fallback.signal_event,
    cond->fallback.broadcast_event
  };

  /* Avoid race conditions. */
  EnterCriticalSection(&cond->fallback.waiters_count_lock);
  cond->fallback.waiters_count++;
  LeaveCriticalSection(&cond->fallback.waiters_count_lock);

  /* It's ok to release the <mutex> here since Win32 manual-reset events */
  /* maintain state when used with <SetEvent>. This avoids the "lost wakeup" */
  /* bug. */
  uv_mutex_unlock(mutex);

  /* Wait for either event to become signaled due to <uv_cond_signal> being */
  /* called or <uv_cond_broadcast> being called. */
  result = WaitForMultipleObjects(2, handles, FALSE, dwMilliseconds);

  EnterCriticalSection(&cond->fallback.waiters_count_lock);
  cond->fallback.waiters_count--;
  last_waiter = result == WAIT_OBJECT_0 + 1
      && cond->fallback.waiters_count == 0;
  LeaveCriticalSection(&cond->fallback.waiters_count_lock);

  /* Some thread called <pthread_cond_broadcast>. */
  if (last_waiter) {
    /* We're the last waiter to be notified or to stop waiting, so reset the */
    /* the manual-reset event. */
    ResetEvent(cond->fallback.broadcast_event);
  }

  /* Reacquire the <mutex>. */
  uv_mutex_lock(mutex);

  if (result == WAIT_OBJECT_0 || result == WAIT_OBJECT_0 + 1)
    return 0;

  if (result == WAIT_TIMEOUT)
    return UV_ETIMEDOUT;

  abort();
  return -1; /* Satisfy the compiler. */
}


