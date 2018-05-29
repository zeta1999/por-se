#include "klee/klee.h"
#include "pthread_impl.h"

#include <stdlib.h>
#include <string.h>
#include <errno.h>

static __pthread_impl_pthread* __obtain_pthread_data(pthread_t pthread) {
  return ((__pthread_impl_pthread*)pthread);
}

static void* __pthread_impl_wrapper(void* arg) {
  __pthread_impl_pthread* thread = arg;
  void* ret = thread->startRoutine(thread->startArg);
  pthread_exit(ret);
}

int pthread_create(pthread_t *pthread, const pthread_attr_t *attr, void *(*startRoutine)(void *), void *arg) {
  __pthread_impl_pthread* thread = malloc(sizeof(__pthread_impl_pthread));
  memset(thread, 0, sizeof(__pthread_impl_mutex));

  klee_mark_thread_shareable(thread);

  *((__pthread_impl_pthread**)pthread) = thread;

  uint64_t tid = (uint64_t) thread;
  thread->tid = tid;
  thread->startRoutine = startRoutine;
  thread->startArg = arg;
  thread->returnValue = NULL;
  thread->state = 0;
  thread->mode = 0;
  thread->cancelState = PTHREAD_CANCEL_ENABLE;
  __stack_create(&thread->waitingForJoinThreads);

  // Since the thread pointer will be on the stack we want to mark it as thread
  // shareable so deduping of threads can actually work
  klee_mark_thread_shareable(&thread);
  klee_create_thread(tid, __pthread_impl_wrapper, thread);

  return 0;
}

int pthread_detach(pthread_t pthread) {
  if (pthread == 0) {
    return 0;
  }

  __pthread_impl_pthread* thread = __obtain_pthread_data(pthread);
  if (thread->mode == 1) {
    return EINVAL;
  }

  thread->mode = 1;
  klee_preempt_thread();

  return 0;
}

void pthread_exit(void* arg) __attribute__ ((__noreturn__)) {
  uint64_t tid = klee_get_thread_id();

  if (tid != 0) { // 0 is the main thread and nobody can join it
    __pthread_impl_pthread* thread = (__pthread_impl_pthread*) tid;
    thread->returnValue = arg;
    thread->state = 1;

    __notify_threads(&thread->waitingForJoinThreads);
  }

  klee_exit_thread();
}

int pthread_join(pthread_t pthread, void **ret) {
  __pthread_impl_pthread* thread = __obtain_pthread_data(pthread);

  if (thread->mode == 1) { // detached state
    return EINVAL;
  }

  if (__stack_size(&thread->waitingForJoinThreads) == 1) {
    return EINVAL;
  }

  uint64_t ownThread = klee_get_thread_id();
  if (ownThread == pthread) { // We refer to our onw thread
    return EDEADLK;
  }

  // Could also be that this thread is already finished, but there must be
  // at least one call to join to free the resources

  if (thread->state != 1) {
    __stack_push(&thread->waitingForJoinThreads, (void*) ownThread);
    klee_sleep_thread();
  } else {
    klee_preempt_thread();
  }

  if (ret != NULL) {
    if (thread->cancelSignalReceived == 1) {
      *ret = PTHREAD_CANCELED;
    } else {
      // If we have returned, then we should be able to return the data
      *ret = thread->returnValue;
    }
  }

  return 0;
}

pthread_t pthread_self(void) {
  return (pthread_t) klee_get_thread_id();
}

int pthread_equal(pthread_t t1, pthread_t t2) {
  return t1 == t2 ? 1 : 0;
}

//
//#ifndef __cplusplus
//#define pthread_equal(x,y) ((x)==(y))
//#endif
//

int pthread_setcancelstate(int state, int *oldState) {
  if (state != PTHREAD_CANCEL_ENABLE && state != PTHREAD_CANCEL_DISABLE) {
    return EINVAL;
  }

  uint64_t tid = klee_get_thread_id();
  if (tid != 0) {
    __pthread_impl_pthread* thread = (__pthread_impl_pthread*) tid;
    *oldState = thread->cancelState;
    thread->cancelState = state;
  }

  return 0;
}

//int pthread_setcanceltype(int, int *);

void pthread_testcancel() {
  uint64_t tid = klee_get_thread_id();
  if (tid == 0) {
    return;
  }

  __pthread_impl_pthread* thread = (__pthread_impl_pthread*) tid;
  if (thread->cancelState == PTHREAD_CANCEL_DISABLE) {
    return;
  }

  if (thread->cancelSignalReceived == 1) {
    pthread_exit(PTHREAD_CANCELED);
  }
}

int pthread_cancel(pthread_t tid) {
  __pthread_impl_pthread* thread = (__pthread_impl_pthread*) tid;
  thread->cancelSignalReceived = 1;

  return 0;
}

//int pthread_key_create(pthread_key_t *, void (*)(void *));
//int pthread_key_delete(pthread_key_t);
//void *pthread_getspecific(pthread_key_t);
//int pthread_setspecific(pthread_key_t, const void *);

//int pthread_attr_init(pthread_attr_t *);
//int pthread_attr_destroy(pthread_attr_t *);

//int pthread_attr_getguardsize(const pthread_attr_t *__restrict, size_t *__restrict);
//int pthread_attr_setguardsize(pthread_attr_t *, size_t);
//int pthread_attr_getstacksize(const pthread_attr_t *__restrict, size_t *__restrict);
//int pthread_attr_setstacksize(pthread_attr_t *, size_t);
//int pthread_attr_getdetachstate(const pthread_attr_t *, int *);
//int pthread_attr_setdetachstate(pthread_attr_t *, int);
//int pthread_attr_getstack(const pthread_attr_t *__restrict, void **__restrict, size_t *__restrict);
//int pthread_attr_setstack(pthread_attr_t *, void *, size_t);
//int pthread_attr_getscope(const pthread_attr_t *__restrict, int *__restrict);
//int pthread_attr_setscope(pthread_attr_t *, int);
//int pthread_attr_getschedpolicy(const pthread_attr_t *__restrict, int *__restrict);
//int pthread_attr_setschedpolicy(pthread_attr_t *, int);
//int pthread_attr_getschedparam(const pthread_attr_t *__restrict, struct sched_param *__restrict);
//int pthread_attr_setschedparam(pthread_attr_t *__restrict, const struct sched_param *__restrict);
//int pthread_attr_getinheritsched(const pthread_attr_t *__restrict, int *__restrict);
//int pthread_attr_setinheritsched(pthread_attr_t *, int);

//int pthread_atfork(void (*)(void), void (*)(void), void (*)(void));

//int pthread_getconcurrency(void);
//int pthread_setconcurrency(int);

//int pthread_getcpuclockid(pthread_t, clockid_t *);