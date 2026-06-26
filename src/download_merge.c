#include "download.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

// 数据合并线程函数
void* download_merge_thread(void* arg) {
    file_download_task *task = (file_download_task*)arg;

    // 等待所有下载线程完成
    while (1) {
        pthread_mutex_lock(&task->lock);
        int all_complete = 1;

        for (int i = 0; i < task->thread_count; i++) {
            if (!task->thread_status[i]) {
                all_complete = 0;
                break;
            }
        }

        pthread_mutex_unlock(&task->lock);

        if (all_complete) {
            break;
        }

        sleep(0.1);
    }

    // 打开文件
    int flags = O_WRONLY | O_CREAT | O_TRUNC;
    int fd = open(task->file_info.name, flags, 0644);
    if (fd == -1) {
        perror("open fail");
        pthread_mutex_lock(&task->lock);
        task->error_code = DOWNLOAD_ERROR_FILE_OPEN;
        snprintf(task->error_msg, sizeof(task->error_msg),
                 "无法打开文件");
        pthread_cond_signal(&task->cond);
        pthread_mutex_unlock(&task->lock);
        return NULL;
    }

    // 写入数据块
    size_t total_written = 0;
    for (int i = 0; i < task->thread_count; i++) {
        if (task->thread_status[i] && task->thread_chunk_data[i]) {
            lseek(fd, i * task->chunk_size, SEEK_SET);
            int written = write(fd, task->thread_chunk_data[i], task->chunk_size);
            if (written < 0) {
                perror("write fail");
                close(fd);

                pthread_mutex_lock(&task->lock);
                task->error_code = DOWNLOAD_ERROR_FILE_OPEN;
                snprintf(task->error_msg, sizeof(task->error_msg),
                         "写入文件失败");
                pthread_cond_signal(&task->cond);
                pthread_mutex_unlock(&task->lock);
                return NULL;
            }
            total_written += written;
        }
    }

    // 验证写入大小
    if (total_written != task->total_size) {
        fprintf(stderr, "写入大小不匹配: 写入 %zu, 期望 %zu\n",
                total_written, task->total_size);
        close(fd);

        pthread_mutex_lock(&task->lock);
        task->error_code = DOWNLOAD_ERROR_UNKNOWN;
        snprintf(task->error_msg, sizeof(task->error_msg),
                 "写入大小不匹配");
        pthread_cond_signal(&task->cond);
        pthread_mutex_unlock(&task->lock);
        return NULL;
    }

    close(fd);

    // 删除断点文件
    delete_resume(task->file_info.name);

    // 通知任务完成
    pthread_mutex_lock(&task->lock);
    task->error_code = DOWNLOAD_SUCCESS;
    task->is_completed = 1;
    printf("\n下载完成: %s\n", task->file_info.name);
    pthread_cond_signal(&task->cond);
    pthread_mutex_unlock(&task->lock);

    return NULL;
}
