#include "download.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

// 初始化下载任务
int init_download_task(file_download_task *task, file_inof_t *file_info, int sock) {
    if (!task || !file_info || sock <= 0) {
        return DOWNLOAD_ERROR_UNKNOWN;
    }

    // 初始化结构体
    memset(task, 0, sizeof(file_download_task));

    // 复制文件信息
    memcpy(&task->file_info, file_info, sizeof(file_inof_t));
    task->sock = sock;
    task->total_size = file_info->size;
    task->received_size = 0;
    task->error_code = DOWNLOAD_SUCCESS;

    // 初始化互斥锁和条件变量
    pthread_mutex_init(&task->lock, NULL);
    pthread_cond_init(&task->cond, NULL);
    task->is_completed = 0;

    // 初始化多线程相关
    task->thread_count = 1;  // 默认单线程
    task->threads = NULL;
    task->thread_status = NULL;
    task->thread_chunk_data = NULL;
    task->chunk_size = buff_size;

    // 检查断点续传
    task->resume_offset = read_download_progress(file_info);
    task->use_resume = (task->resume_offset > 0);

    return DOWNLOAD_SUCCESS;
}

// 销毁下载任务
void destroy_download_task(file_download_task *task) {
    if (!task) {
        return;
    }

    // 释放线程相关资源
    if (task->threads) {
        free(task->threads);
    }
    if (task->thread_status) {
        free(task->thread_status);
    }
    if (task->thread_chunk_data) {
        for (int i = 0; i < task->thread_count; i++) {
            if (task->thread_chunk_data[i]) {
                free(task->thread_chunk_data[i]);
            }
        }
        free(task->thread_chunk_data);
    }

    // 销毁同步对象
    pthread_mutex_destroy(&task->lock);
    pthread_cond_destroy(&task->cond);
}

// 启动单个文件的多线程下载
int download_file_with_threads(file_download_task *task) {
    if (!task) {
        return DOWNLOAD_ERROR_UNKNOWN;
    }

    // 初始化下载配置
    download_config_t config;
    if (init_download_config(&config, task->total_size) != DOWNLOAD_SUCCESS) {
        task->error_code = DOWNLOAD_ERROR_UNKNOWN;
        snprintf(task->error_msg, sizeof(task->error_msg), "初始化下载配置失败");
        return DOWNLOAD_ERROR_UNKNOWN;
    }

    // 如果是单线程且不需要断点续传，使用原有方式
    if (config.thread_count == 1 && !task->use_resume) {
        printf("使用单线程下载: %s\n", task->file_info.name);
        // 调用原有的recve函数（简化实现）
        // 这里需要修改原有的recve函数以支持多线程，暂时使用单线程
        return download_file_single_thread(task);
    }

    printf("使用多线程下载: %s\n", task->file_info.name);
    printf("线程数: %zu\n", config.thread_count);
    printf("块大小: %zu bytes\n", config.block_size);

    // 为每个线程分配缓冲区
    task->threads = malloc(config.thread_count * sizeof(pthread_t));
    task->thread_status = calloc(config.thread_count, sizeof(int));
    task->thread_chunk_data = malloc(config.thread_count * sizeof(char*));
    task->chunk_size = config.block_size;

    if (!task->threads || !task->thread_status || !task->thread_chunk_data) {
        perror("malloc fail");
        task->error_code = DOWNLOAD_ERROR_THREAD_CREATE;
        snprintf(task->error_msg, sizeof(task->error_msg), "内存分配失败");
        return DOWNLOAD_ERROR_THREAD_CREATE;
    }

    for (int i = 0; i < config.thread_count; i++) {
        task->thread_chunk_data[i] = malloc(config.block_size);
        if (!task->thread_chunk_data[i]) {
            perror("malloc fail");

            // 清理已分配的内存
            for (int j = 0; j < i; j++) {
                if (task->thread_chunk_data[j]) {
                    free(task->thread_chunk_data[j]);
                }
            }
            free(task->thread_chunk_data);
            free(task->thread_status);
            free(task->threads);

            task->error_code = DOWNLOAD_ERROR_THREAD_CREATE;
            snprintf(task->error_msg, sizeof(task->error_msg), "内存分配失败");
            return DOWNLOAD_ERROR_THREAD_CREATE;
        }
    }

    // 创建下载工作线程
    for (int i = 0; i < config.thread_count; i++) {
        download_worker_arg_t *arg = malloc(sizeof(download_worker_arg_t));
        if (!arg) {
            perror("malloc fail");
            task->error_code = DOWNLOAD_ERROR_THREAD_CREATE;
            snprintf(task->error_msg, sizeof(task->error_msg), "内存分配失败");
            return DOWNLOAD_ERROR_THREAD_CREATE;
        }

        arg->task = task;
        arg->thread_id = i;
        arg->offset = i * config.block_size;
        arg->chunk_size = config.block_size;

        if (pthread_create(&task->threads[i], NULL,
                          download_worker_thread, arg) != 0) {
            perror("pthread_create");
            free(arg);

            // 清理已创建的线程
            for (int j = 0; j < i; j++) {
                pthread_cancel(task->threads[j]);
            }

            // 清理内存
            for (int j = 0; j < config.thread_count; j++) {
                if (task->thread_chunk_data[j]) {
                    free(task->thread_chunk_data[j]);
                }
            }
            free(task->thread_chunk_data);
            free(task->thread_status);
            free(task->threads);

            task->error_code = DOWNLOAD_ERROR_THREAD_CREATE;
            snprintf(task->error_msg, sizeof(task->error_msg), "线程创建失败");
            return DOWNLOAD_ERROR_THREAD_CREATE;
        }
    }

    // 创建合并线程
    pthread_t merge_thread;
    if (pthread_create(&merge_thread, NULL, download_merge_thread, task) != 0) {
        perror("pthread_create");
        task->error_code = DOWNLOAD_ERROR_THREAD_CREATE;
        snprintf(task->error_msg, sizeof(task->error_msg), "合并线程创建失败");
        return DOWNLOAD_ERROR_THREAD_CREATE;
    }

    // 等待合并线程完成（内部会等待下载线程完成）
    pthread_join(merge_thread, NULL);

    // 等待下载线程结束（取消它们）
    for (int i = 0; i < config.thread_count; i++) {
        pthread_cancel(task->threads[i]);
    }

    // 清理线程
    for (int i = 0; i < config.thread_count; i++) {
        pthread_join(task->threads[i], NULL);
    }

    // 清理内存
    for (int i = 0; i < config.thread_count; i++) {
        if (task->thread_chunk_data[i]) {
            free(task->thread_chunk_data[i]);
        }
    }
    free(task->thread_chunk_data);
    free(task->thread_status);
    free(task->threads);

    return task->error_code;
}

// 单线程下载（简化实现，用于向后兼容）
int download_file_single_thread(file_download_task *task) {
    // 这里使用原有的recve函数
    // 注意：需要修改recve函数以支持多线程任务的接口
    // 为了简化，这里暂时不实现完整的单线程下载

    printf("单线程下载功能暂未实现\n");
    task->error_code = DOWNLOAD_ERROR_UNKNOWN;
    snprintf(task->error_msg, sizeof(task->error_msg), "单线程下载功能暂未实现");

    return DOWNLOAD_ERROR_UNKNOWN;
}

// 启动多文件并发下载
int start_multi_file_download(int sock, file_inof_t *files, int file_count) {
    if (!files || file_count <= 0 || sock <= 0) {
        return DOWNLOAD_ERROR_UNKNOWN;
    }

    printf("准备下载 %d 个文件\n", file_count);

    // 创建下载任务
    file_download_task *tasks = malloc(file_count * sizeof(file_download_task));
    if (!tasks) {
        perror("malloc fail");
        return DOWNLOAD_ERROR_UNKNOWN;
    }

    // 初始化任务
    for (int i = 0; i < file_count; i++) {
        if (init_download_task(&tasks[i], &files[i], sock) != DOWNLOAD_SUCCESS) {
            fprintf(stderr, "初始化任务 %d 失败\n", i);
            for (int j = 0; j < i; j++) {
                destroy_download_task(&tasks[j]);
            }
            free(tasks);
            return DOWNLOAD_ERROR_UNKNOWN;
        }

        // 打印文件信息
        printf("\n文件 %d:\n", i + 1);
        printf("  名称: %s\n", tasks[i].file_info.name);
        printf("  大小: %zu bytes\n", tasks[i].total_size);
        printf("  断点续传: %s\n", tasks[i].use_resume ? "是" : "否");
        if (tasks[i].use_resume) {
            printf("  从 %zu 字节续传\n", tasks[i].resume_offset);
        }
    }

    // 启动所有文件的下载
    printf("\n开始下载...\n");
    for (int i = 0; i < file_count; i++) {
        int result = download_file_with_threads(&tasks[i]);
        if (result != DOWNLOAD_SUCCESS) {
            fprintf(stderr, "文件 %s 下载失败: %s\n",
                    tasks[i].file_info.name,
                    tasks[i].error_msg);
        }
    }

    // 清理资源
    for (int i = 0; i < file_count; i++) {
        destroy_download_task(&tasks[i]);
    }
    free(tasks);

    return DOWNLOAD_SUCCESS;
}

// 保存下载进度
void save_download_progress(file_download_task *task) {
    if (!task) {
        return;
    }

    char resume_file[512];
    snprintf(resume_file, sizeof(resume_file), "%s.resume", task->file_info.name);
    int fd = open(resume_file, O_WRONLY | O_CREAT, 0644);
    if (fd == -1) {
        perror("open resume file fail");
        return;
    }
    write(fd, &task->received_size, sizeof(task->received_size));
    close(fd);
}

// 读取下载进度
size_t read_download_progress(file_inof_t *file_info) {
    char resume_file[512];
    snprintf(resume_file, sizeof(resume_file), "%s.resume", file_info->name);
    int fd = open(resume_file, O_RDONLY);
    if (fd == -1) {
        return 0;
    }
    size_t received_size = 0;
    read(fd, &received_size, sizeof(received_size));
    close(fd);

    // 检查进度是否有效
    struct stat st;
    if (stat(file_info->name, &st) == 0) {
        if (st.st_size > received_size) {
            return st.st_size;  // 使用实际文件大小
        }
    }

    return received_size;
}
