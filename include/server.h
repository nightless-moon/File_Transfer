#ifndef SERVER_H
#define SERVER_H
#include <stdio.h>
#include <string.h>
#include <pthread.h>
#include <sys/socket.h>
#include <stdint.h>
#include <arpa/inet.h> 
#include <unistd.h>    
#include <fcntl.h>  
#include <sys/types.h> 
#include <sys/stat.h>
#include <sys/time.h>
#include <stdlib.h>
#define ker 1234

#pragma pack(push, 1)
typedef struct 
{
    char name[101];        // 文件名
    uint32_t mode;         // 文件模式 
    uint64_t size;         // 文件大小
} file_info;
#pragma pack(pop)

typedef struct
{
    char ip[16];            // 客户端ip地址
    int sock_conn;          // 客户端套接字
    unsigned short port;    // 客户端端口号
	time_t online_time;     // 上线时间
	char** send_file_list;  // 待发送的文件路径列表
	int send_file_cnt;      // 待发送的文件数量
	int is_authenticated;  // 是否已认证
	char username[50];      // 认证后的用户名
} user;
void* comm_thr(void* arg);
int send_file(int sock, const char* file_path);

#endif
