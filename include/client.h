#ifndef CLIENT_H
#define CLIENT_H

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <stdint.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/stat.h>
#include <errno.h>
#include <sys/time.h>
#define buff_size 4096
#pragma pack(push, 1)

typedef struct 
{
    char name[101];      //文件名
    uint32_t mode;       //文件模式
    uint64_t size;       //文件大小
} file_inof_t;

typedef struct 
{
    file_inof_t file;       //文件信息
    size_t received_size;   //已接收的字节数
} received_file;


#pragma pack(pop)
int recve(int sock, file_inof_t* file, char* buff);

// 多线程下载相关
typedef struct
{
    file_inof_t file_info;
    int sock;
    uint64_t total_size;
    uint64_t received_size;
    pthread_mutex_t lock;
    pthread_cond_t cond;
    int is_completed;
    int error_code;
    char error_msg[256];
    pthread_t *threads;
    int thread_count;
    int *thread_status;
    char **thread_chunk_data;
    uint64_t chunk_size;
    size_t resume_offset;
    int use_resume;
} file_download_task;

int start_multi_file_download(int sock, file_inof_t *files, int file_count);
int download_file_with_threads(file_download_task *task);
void destroy_download_task(file_download_task *task);
int calculate_thread_count(uint64_t file_size);
int init_download_config(void *config, uint64_t file_size);

#endif
