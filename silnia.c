#include <stdio.h>

#include "threadpool.h"
#include "future.h"

#define POOL_SIZE 3

void *bucket_prep(void *arg, size_t argsz __attribute__((unused)), size_t *size) {
  int *pair;
  int num = *(int *) arg;

  if ((pair = malloc(sizeof(int) * 2)) == NULL) exit(12);
  if (num) {
    pair[0] = num;
  } else {
    pair[0] = 1;
  }
  pair[1] = num;
  *size = sizeof(int) * 2;

  return (void *) pair;
}

void *nth_factorial(void *arg, size_t argsz, size_t *size) {
  int *factorial = (int *) arg;
  factorial[1] += POOL_SIZE;
  factorial[0] *= factorial[1];
  *size = argsz;

  return (void *) factorial;
}

int main() {
  int n;
  int err;
  unsigned long long factorial = 1;
  thread_pool_t pool;

  thread_pool_init(&pool, POOL_SIZE);
  scanf("%d", &n);
  future_t future[n + 1];
  int bucket[POOL_SIZE];
  for (size_t i = 0; i < POOL_SIZE; i++) {
    bucket[i] = i;
  }

  for (int i = 0; i < POOL_SIZE; i++) {
    if (i > n) break;
    callable_t callable;
    callable.arg = &bucket[i];
    callable.argsz = sizeof(int);
    callable.function = bucket_prep;
    if ((err = async(&pool, &future[i], callable)) != 0) return err;
    for (int j = i + POOL_SIZE; j <= n; j += POOL_SIZE) {
      if ((err = map(&pool, &future[j], &future[j - POOL_SIZE], nth_factorial)) != 0) return err;
    }
  }

  int *nth_factorial_value;
  for (int i = 0; i < POOL_SIZE; i++) {
    if (i > n) break;
    nth_factorial_value = await(&future[n - i]);
    factorial *= nth_factorial_value[0];
    free(nth_factorial_value);
  }

  thread_pool_destroy(&pool);
  printf("%llu\n", factorial);

  return 0;
}
