#ifndef THREAD_POOL_H
#define THREAD_POOL_H

#include <pthread.h>
#include "download.h"

// 任务队列
typedef struct {
    file_download_task *task;
    uint64_t offset;
    size_t chunk_size;
} task_queue_item_t;

// 线程池
typedef struct {
    pthread_t *threads;
    int thread_count;
    pthread_mutex_t lock;
    pthread_cond_t cond;

    task_queue_item_t *queue;
    int queue_size;
    int queue_head;
    int queue_tail;
    int max_queue_size;
} thread_pool_t;

// 线程池操作函数
int thread_pool_init(thread_pool_t *pool, int thread_count, int max_queue_size);
int thread_pool_enqueue(thread_pool_t *pool, task_queue_item_t *item);
int thread_pool_wait(thread_pool_t *pool);
void thread_pool_destroy(thread_pool_t *pool);
void thread_pool_shutdown(thread_pool_t *pool);

// 工作线程函数
void* thread_pool_worker(void* arg);

#endif
