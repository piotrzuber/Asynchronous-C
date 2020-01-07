#include <pthread.h>
#include <signal.h>
#include <stdio.h>
#include "threadpool.h"
#include "err.h"

char sigint_catched = 0;

void sig_handler(int signo) {
  if (signo == SIGINT) {
    sigint_catched = 1;
  }
}

char end(thread_pool_t *pool) {
  int err;
  if (pool->cancelled && pool->q_back == NULL) {
    if ((err = pthread_cond_broadcast(&pool->cond)) != 0) syserr(err, "cond_signal");
    if ((err = pthread_mutex_unlock(&pool->mtx)) != 0) syserr(err, "mutex_unlock");

    return 1;
  }

  return 0;
}

static task_queue_t *queue_push(thread_pool_t *pool, runnable_t task) {
  task_queue_t *new = malloc(sizeof(task_queue_t));
  if (new) {
    new->runnable = task;
    new->next = NULL;
    if (pool->q_back) {
      pool->q_back->next = new;
      pool->q_back = new;
    } else {
      pool->q_front = pool->q_back = new;
      pool->q_front->next = pool->q_back->next = NULL;
    }
  }

  return new;
}

static task_queue_t *queue_pop(thread_pool_t *pool) {
  task_queue_t *front = pool->q_front;
  if (pool->q_front == pool->q_back) {
    pool->q_front = pool->q_back = NULL;
  } else {
    pool->q_front = pool->q_front->next;
  }

  return front;
}

static void *thread(void *arg) {
  task_queue_t *task;
  runnable_t runnable;
  int err;
  thread_pool_t *pool = (thread_pool_t *) arg;

  while (1) {
    if ((err = pthread_mutex_lock(&pool->mtx)) != 0) syserr(err, "mutex_lock");
    if (end(pool)) {
      return NULL;
    }
    while (!pool->cancelled && pool->q_front == NULL) {
      if ((err = pthread_cond_wait(&pool->cond, &pool->mtx)) != 0) syserr(err, "cond_wait");
    }
    if (end(pool)) {
      return NULL;
    }

    task = queue_pop(pool);
    runnable = task->runnable;

    if ((err = pthread_mutex_unlock(&pool->mtx)) != 0) syserr(err, "mutex_unlock");

    runnable.function(runnable.arg, runnable.argsz);

    free(task);
  }
}

int thread_pool_init(thread_pool_t *pool, size_t num_threads) {
  pthread_attr_t attr;
  int err;

  if ((err = pthread_attr_init(&attr)) != 0) syserr(err, "attr_init");
  if ((err = pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_JOINABLE)) != 0) {
    syserr(err, "attr_setdetachstate");
  }
  if ((err = pthread_mutex_init(&pool->mtx, 0)) != 0) syserr(err, "mutex_init");
  if ((err = pthread_cond_init(&pool->cond, 0)) != 0) syserr(err, "cond_init");
  pool->nthreads = num_threads;
  pool->cancelled = 0;
  pool->q_front = pool->q_back = NULL;
  if ((pool->threads = malloc(num_threads * sizeof(pthread_t))) == NULL) return MALLOC_FAILURE;

  for (size_t i = 0; i < num_threads; i++) {
    pthread_create(&pool->threads[i], &attr, &thread, pool);
  }
  if ((err = pthread_attr_destroy(&attr)) != 0) syserr(err, "attr_destroy");

  return 0;
}

void thread_pool_destroy(struct thread_pool *pool) {
  int err;
  if ((err = pthread_mutex_lock(&pool->mtx)) != 0) syserr(err, "mutex_lock");
  pool->cancelled = 1;
  if ((err = pthread_cond_broadcast(&pool->cond)) != 0) syserr(err, "cond_broadcast");
  if ((err = pthread_mutex_unlock(&pool->mtx)) != 0) syserr(err, "mutex_unlock");

  for (size_t i = 0; i < pool->nthreads; i++) {
    if ((err = pthread_join(pool->threads[i], NULL)) != 0) syserr(err, "join");
  }
  if ((err = pthread_mutex_destroy(&pool->mtx)) != 0) syserr(err, "mutex_destroy");
  if ((err = pthread_cond_destroy(&pool->cond)) != 0) syserr(err, "cond_destroy");
  free(pool->threads);
}

int defer(struct thread_pool *pool, runnable_t runnable) {
  int err;
  if ((err = pthread_mutex_lock(&pool->mtx)) != 0) syserr(err, "mutex_lock");
  if (sigint_catched && !pool->cancelled) {
    pool->cancelled = 1;
    if ((err = pthread_mutex_unlock(&pool->mtx)) != 0) syserr(err, "mutex_unlock");

    thread_pool_destroy(pool);
    return -1;
  } else if (!pool->cancelled) {
    if (!queue_push(pool, runnable)) {
      if ((err = pthread_mutex_unlock(&pool->mtx)) != 0) syserr(err, "mutex_unlock");
      return MALLOC_FAILURE;
    }
    if ((err = pthread_cond_signal(&pool->cond)) != 0) syserr(err, "cond_signal");
  } else {
    if ((err = pthread_mutex_unlock(&pool->mtx)) != 0) syserr(err, "mutex_unlock");

    return -1;
  }
  if ((err = pthread_mutex_unlock(&pool->mtx)) != 0) syserr(err, "mutex_unlock");

  return 0;
}
