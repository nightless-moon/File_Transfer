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
#define buff_size 1024
#pragma pack(push, 1)

typedef struct 
{
    char name[101];      //文件名
    uint32_t mode;       //文件模式
    uint64_t size;       //文件大小
} file_inof_t;

#pragma pack(pop)
int recve(int sock, file_inof_t* file, char* buff);

#endif
