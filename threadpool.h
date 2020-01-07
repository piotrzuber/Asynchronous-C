#ifndef THREADPOOL_H
#define THREADPOOL_H

#include <stddef.h>
#include <stdlib.h>

#define MALLOC_FAILURE -12

typedef struct runnable {
  void (*function)(void *, size_t);
  void *arg;
  size_t argsz;
} runnable_t;

typedef struct thread_pool {
  char cancelled;
  unsigned int nthreads;
  struct task_queue *q_front;
  struct task_queue *q_back;
  pthread_mutex_t mtx;
  pthread_cond_t cond;
  pthread_t *threads;
} thread_pool_t;

typedef struct task_queue {
  struct runnable runnable;
  struct task_queue *next;
} task_queue_t;

int thread_pool_init(thread_pool_t *pool, size_t pool_size);

void thread_pool_destroy(thread_pool_t *pool);

int defer(thread_pool_t *pool, runnable_t runnable);

#endif
