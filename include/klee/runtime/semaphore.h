#ifndef _SEMAPHORE_H
#define _SEMAPHORE_H

#include <time.h>
#include <stdint.h>

#include "pthread.h"

#define SEM_FAILED ((sem_t *) 0)

#ifdef SEM_VALUE_MAX
#undef SEM_VALUE_MAX
#endif
#define SEM_VALUE_MAX (32767)

typedef struct {
  pthread_internal_t magic;

  int value;
  const char* name;

  size_t waitingCount;

  void* mutex;
  void* cond;
} sem_t;

int sem_init(sem_t *sem, int f, unsigned v);
int sem_destroy(sem_t * sem);

sem_t *sem_open(const char *name, int f, ...);
int sem_unlink(const char *name);
int sem_close(sem_t *sem);

int sem_getvalue(sem_t *sem, int *v);
int sem_post(sem_t *sem);
int sem_timedwait(sem_t *sem, const struct timespec *time);
int sem_trywait(sem_t *sem);
int sem_wait(sem_t *sem);


#endif // _SEMAPHORE_H
