#include "download.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

// 下载工作线程参数
typedef struct {
    file_download_task *task;
    int thread_id;
    uint64_t offset;
    size_t chunk_size;
} download_worker_arg_t;

// 下载工作线程函数
void* download_worker_thread(void* arg) {
    download_worker_arg_t *args = (download_worker_arg_t*)arg;
    file_download_task *task = args->task;
    uint64_t offset = args->offset;
    size_t chunk_size = args->chunk_size;

    // 计算本次请求的数据大小
    size_t to_request = chunk_size;
    if (offset + chunk_size > task->total_size) {
        to_request = task->total_size - offset;
    }

    if (to_request <= 0) {
        pthread_mutex_lock(&task->lock);
        task->error_code = DOWNLOAD_ERROR_UNKNOWN;
        snprintf(task->error_msg, sizeof(task->error_msg), "无效的偏移量或大小");
        pthread_mutex_unlock(&task->lock);
        free(args);
        return NULL;
    }

    // 发送下载请求
    char request[512];
    snprintf(request, sizeof(request),
             "DOWNLOAD_BLOCK %s %zu %zu",
             task->file_info.name, offset, to_request);

    if (send(task->sock, request, strlen(request), 0) <= 0) {
        pthread_mutex_lock(&task->lock);
        task->error_code = DOWNLOAD_ERROR_SEND;
        snprintf(task->error_msg, sizeof(task->error_msg),
                 "发送下载请求失败");
        pthread_mutex_unlock(&task->lock);
        free(args);
        return NULL;
    }

    // 接收数据
    char *buffer = malloc(to_request);
    if (!buffer) {
        pthread_mutex_lock(&task->lock);
        task->error_code = DOWNLOAD_ERROR_FILE_OPEN;
        snprintf(task->error_msg, sizeof(task->error_msg),
                 "内存分配失败");
        pthread_mutex_unlock(&task->lock);
        free(args);
        return NULL;
    }

    int rec = recv(task->sock, buffer, to_request, 0);
    if (rec <= 0) {
        free(buffer);
        pthread_mutex_lock(&task->lock);
        task->error_code = DOWNLOAD_ERROR_RECV;
        snprintf(task->error_msg, sizeof(task->error_msg),
                 "接收数据失败");
        pthread_mutex_unlock(&task->lock);
        free(args);
        return NULL;
    }

    // 保存数据到线程缓冲区
    pthread_mutex_lock(&task->lock);
    if (task->thread_chunk_data && task->thread_chunk_data[args->thread_id]) {
        free(task->thread_chunk_data[args->thread_id]);
    }
    task->thread_chunk_data[args->thread_id] = buffer;
    task->thread_status[args->thread_id] = 1;
    task->received_size += rec;

    // 更新进度
    printf("下载进度: %.1f%%\r",
           (double)task->received_size / task->total_size * 100);
    fflush(stdout);

    // 保存断点
    save_download_progress(task);

    pthread_mutex_unlock(&task->lock);

    // 通知任务完成
    pthread_mutex_lock(&task->lock);
    task->is_completed = 1;
    pthread_cond_signal(&task->cond);
    pthread_mutex_unlock(&task->lock);

    free(args);
    return NULL;
}
