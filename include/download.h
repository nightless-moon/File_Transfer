#ifndef DOWNLOAD_H
#define DOWNLOAD_H

#include "client.h"
#include <pthread.h>
#include <stdint.h>

// 下载配置
typedef struct {
    uint64_t thread_count;         // 线程数
    uint64_t block_size;           // 每个线程负责的块大小
    uint64_t chunk_size;           // 每次读取/发送的数据块大小
    int use_multi_thread;          // 是否启用多线程
} download_config_t;

// 错误代码
#define DOWNLOAD_SUCCESS 0
#define DOWNLOAD_ERROR_CONNECTION -1
#define DOWNLOAD_ERROR_RECV -2
#define DOWNLOAD_ERROR_SEND -3
#define DOWNLOAD_ERROR_FILE_OPEN -4
#define DOWNLOAD_ERROR_THREAD_CREATE -5
#define DOWNLOAD_ERROR_TIMEOUT -6
#define DOWNLOAD_ERROR_NETWORK -7
#define DOWNLOAD_ERROR_UNKNOWN -99

// 下载错误消息
#define DOWNLOAD_ERROR_MSG(connection) "连接已断开"
#define DOWNLOAD_ERROR_MSG(recv) "接收数据失败"
#define DOWNLOAD_ERROR_MSG(send) "发送数据失败"
#define DOWNLOAD_ERROR_MSG(file_open) "打开文件失败"
#define DOWNLOAD_ERROR_MSG(thread_create) "线程创建失败"
#define DOWNLOAD_ERROR_MSG(timeout) "操作超时"
#define DOWNLOAD_ERROR_MSG(network) "网络错误"
#define DOWNLOAD_ERROR_MSG(unknown) "未知错误"

// 文件下载任务
typedef struct {
    file_inof_t file_info;
    int sock;
    uint64_t total_size;
    uint64_t received_size;
    pthread_mutex_t lock;
    pthread_cond_t cond;
    int is_completed;
    int error_code;
    char error_msg[256];

    // 多线程相关
    pthread_t *threads;
    int thread_count;
    int *thread_status;
    char **thread_chunk_data;
    uint64_t chunk_size;

    // 断点续传
    size_t resume_offset;
    int use_resume;
} file_download_task;

// 全局函数声明
int calculate_thread_count(uint64_t file_size);
int init_download_config(download_config_t *config, uint64_t file_size);

int start_multi_file_download(int sock, file_inof_t *files, int file_count);
int download_file_with_threads(file_download_task *task);
void destroy_download_task(file_download_task *task);

// 下载配置管理
int init_download_config(download_config_t *config, uint64_t file_size);
void destroy_download_config(download_config_t *config);

// 文件下载任务管理
int init_download_task(file_download_task *task, file_inof_t *file_info, int sock);
void destroy_download_task(file_download_task *task);

// 进度保存和恢复
void save_download_progress(file_download_task *task);
size_t read_download_progress(file_inof_t *file_info);

#endif
