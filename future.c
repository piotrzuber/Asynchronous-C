#include "future.h"
#include "err.h"

typedef void *(*function_t)(void *);

typedef struct runnable_arg {
  struct callable callable;
  struct future *future;
} runnable_arg_t;

typedef struct map_arg {
  struct future *from;
  void *(*mapping_fn)(void *, size_t, size_t *);
} map_arg_t;

static void runnable_wrapper(void *runnable_arg, size_t argsz __attribute__((unused))) {
  int err;
  runnable_arg_t *arg = runnable_arg;
  future_t *future = arg->future;
  callable_t callable = arg->callable;
  void *value = callable.function(callable.arg, callable.argsz, &future->size);
  future->value = value;

  if ((err = sem_post(&future->status)) != 0) syserr(err, "sem_post");
  free(runnable_arg);
}

static void *map_wrapper(void *map_arg, size_t argsz __attribute__((unused)), size_t *size) {
  map_arg_t *arg = map_arg;

  void *from_result = await(arg->from);
  void *result = arg->mapping_fn(from_result, arg->from->size, size);

  free(map_arg);
  return result;
}

int async(thread_pool_t *pool, future_t *future, callable_t callable) {
  int err;
  runnable_t runnable;
  runnable_arg_t *runnable_arg;
  if ((err = sem_init(&future->status, 0, 0)) != 0) syserr(err, "sem_init");
  if ((runnable_arg = malloc(sizeof(*runnable_arg))) == NULL) return MALLOC_FAILURE;
  runnable_arg->future = future;
  runnable_arg->callable = callable;
  runnable.function = runnable_wrapper;
  runnable.arg = (void *) runnable_arg;
  runnable.argsz = sizeof(*runnable_arg);

  return defer(pool, runnable);
}

int map(thread_pool_t *pool, future_t *future, future_t *from,
        void *(*function)(void *, size_t, size_t *)) {
  map_arg_t *map_arg;
  callable_t callable;
  if ((map_arg = malloc(sizeof(*map_arg))) == NULL) return MALLOC_FAILURE;
  map_arg->from = from;
  map_arg->mapping_fn = function;
  callable.arg = map_arg;
  callable.argsz = sizeof(*map_arg);
  callable.function = map_wrapper;

  return async(pool, future, callable);
}

void *await(future_t *future) {
  int err;
  void *result;

  if ((err = sem_wait(&future->status)) != 0) syserr(err, "sem_wait");
  result = future->value;
  if ((err = sem_destroy(&future->status)) != 0) syserr(err, "mutex_unlock");

  return result;
}
