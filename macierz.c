#include <stdio.h>
#include <unistd.h>
#include <pthread.h>

#include "threadpool.h"

#define MS_TO_U(x) (1000 * x)
#define POOL_SIZE 4

typedef struct cell {
  int value;
  int nap;
  int *row_sum;
  pthread_mutex_t *mtx;
} cell_t;

typedef struct row {
  int *sum;
  pthread_mutex_t *mtx;
} row_t;

int init_rows_data(row_t* data, size_t rows) {
  int err;
  for (size_t i = 0; i < rows; i++) {
    data[i].sum = malloc(sizeof(int));
    data[i].mtx = malloc(sizeof(pthread_mutex_t));
    if ((err = pthread_mutex_init(data[i].mtx, 0)) != 0) return err;
    *(data[i].sum) = 0;
  }

  return 0;
}

void sum(void *arg, size_t argsz __attribute__((unused))) {
  int err;
  cell_t *cell = arg;
  if ((err = usleep(MS_TO_U(cell->nap))) != 0) exit(err);

  if ((err = pthread_mutex_lock(cell->mtx)) != 0) exit(err);
  *cell->row_sum += cell->value;
  if ((err = pthread_mutex_unlock(cell->mtx)) != 0) exit(err);
}

void process(thread_pool_t *pool, cell_t *matrix, row_t *row_data, size_t rows, size_t cols) {
  int val, nap;
  runnable_t runnable;

  for (size_t i = 0; i < rows * cols; i++) {
    scanf("%d %d", &val, &nap);
    matrix[i].value = val;
    matrix[i].nap = nap;
    matrix[i].row_sum = row_data[i / cols].sum;
    matrix[i].mtx = row_data[i / cols].mtx;

    runnable.function = sum;
    runnable.arg = &matrix[i];
    runnable.argsz = sizeof(cell_t);

    defer(pool, runnable);
  }
}

int main() {
  int err;
  size_t rows, cols;

  thread_pool_t pool;
  thread_pool_init(&pool, POOL_SIZE);
  scanf("%zd %zd", &rows, &cols);
  row_t row_data[rows];
  init_rows_data(row_data, rows);
  cell_t matrix[rows * cols];

  process(&pool, matrix, row_data, rows, cols);

  thread_pool_destroy(&pool);

  for (size_t i = 0; i < rows; i++) {
    if ((err = pthread_mutex_destroy(row_data[i].mtx)) != 0) return err;
    printf("%d\n", *row_data[i].sum);
  }

  return 0;
}