#ifndef FUTURE_H
#define FUTURE_H

#include <semaphore.h>

#include "threadpool.h"

typedef struct callable {
  void *(*function)(void *, size_t, size_t *);
  void *arg;
  size_t argsz;
} callable_t;

typedef struct future {
  void *value;
  size_t size;
  sem_t status;
} future_t;

int async(thread_pool_t *pool, future_t *future, callable_t callable);

// Zaproponowana implementacja zakłada jednokrotne wywoływanie
// poniższych funkcji na zmiennej typu future.
// W przypadku wielokrotnych ich wywołań na tym samym obiekcie,
// zachowanie programu pozostaje niezdefiniowane
int map(thread_pool_t *pool, future_t *future, future_t *from,
        void *(*function)(void *, size_t, size_t *));

void *await(future_t *future);

#endif
