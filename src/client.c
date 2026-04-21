#include "client.h"
file_inof_t file;        //存放接收到的文件信息

int main(int argc, char** argv)
{
    //创建套接字
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if(sock == -1)
    {
        perror("socket fail");
        exit(1);
    }

    //服务端的结构信息
    struct sockaddr_in srv_addr;
    srv_addr.sin_addr.s_addr = inet_addr(argv[1]);
    srv_addr.sin_family = AF_INET;
    srv_addr.sin_port = htons(atoi(argv[2]));

    //连接服务端
    if(connect(sock, (struct sockaddr*)&srv_addr, sizeof(srv_addr)) == -1)
    {
        perror("connect fail");
        exit(1);
    }

    //接收数据
    char buff[1024];
    int rec;
    //接收文件信息
    rec = recve(sock, &file, buff);
    if(rec == 0)
    {
        printf("\n接收文件成功!\n文件名:%s\n文件大小:%ld\n",file.name, file.size);
    }

    return 0;
}

int recve(int sock, file_inof_t* file, char* buff)
{
    int rec;
    rec = recv(sock, file, sizeof(file_inof_t), 0);
    if(rec != sizeof(file_inof_t))
    {
        perror("recv fail");
        exit(1);
    }

    int fd = open(file->name, O_WRONLY | O_CREAT, 0644);
   size_t total_received = 0;
    while(total_received < file->size)
    {
        // 计算本次应该接收的字节数
        size_t to_recv = buff_size;
        if(to_recv > file->size - total_received) {
            to_recv = file->size - total_received;
        }
        
        rec = recv(sock, buff, to_recv, 0);
        if(rec <= 0)  // 连接关闭或错误
        {
            if(rec == 0)
                fprintf(stderr, "Connection closed prematurely\n");
            else
                perror("recv fail");
            close(fd);
            return -1;
        }
        
        // 写入文件（只写入实际接收的字节数）
        int write_rec = write(fd, buff, rec);
        if(write_rec != rec)
        {
            perror("write fail");
            close(fd);
            return -1;
        }
        
        total_received += rec;
        printf("Progress: %.1f%%\r", (double)total_received / file->size * 100);
        fflush(stdout);
    }
    
    printf("\n");
    close(fd);
    return 0;


}