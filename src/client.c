#include "client.h"
#include "resume.h"
#include "auth.h"
#include <string.h>

file_inof_t file;        //存放接收到的文件信息
received_file rcv_file;  //存放接收到的文件信息和已接收的字节数

int main(int argc, char** argv)
{
    char server_ip[16];
    char server_port[10];

    // 获取服务器地址
    printf("请输入服务器IP: ");
    fgets(server_ip, sizeof(server_ip), stdin);
    server_ip[strcspn(server_ip, "\n")] = '\0';

    printf("请输入服务器端口: ");
    fgets(server_port, sizeof(server_port), stdin);
    server_port[strcspn(server_port, "\n")] = '\0';

    // 创建套接字
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if(sock == -1)
    {
        perror("socket fail");
        exit(1);
    }

    //服务端的结构信息
    struct sockaddr_in srv_addr;
    srv_addr.sin_addr.s_addr = inet_addr(server_ip);
    srv_addr.sin_family = AF_INET;
    srv_addr.sin_port = htons(atoi(server_port));

    //连接服务端
    if(connect(sock, (struct sockaddr*)&srv_addr, sizeof(srv_addr)) == -1)
    {
        perror("connect fail");
        exit(1);
    }
    else
    {
        printf("连接服务器成功!\n");
    }

    // 认证阶段
    int auth_result = handle_auth_response(sock);
    if (auth_result != 0) {
        fprintf(stderr, "认证失败: %s\n", auth_result == -1 ? "注册失败" : (auth_result == -2 ? "登录失败" : "未知错误"));
        close(sock);
        exit(1);
    }

    // 接收数据
    char buff[buff_size];
    int rec;
    //接收文件信息
    while(1)
    {
        rec = recve(sock, &file, buff);
        if(rec == 0)
        {
            printf("\n接收文件成功!\n文件名:%s\n文件大小:%ld\n",file.name, file.size);
            delete_resume(rcv_file.file.name);
        }
        else if(rec == -1)
        {
            fprintf(stderr, "接收文件失败!\n");
        }
        else if(rec == -2)
        {
            printf("没有新文件了，连接关闭\n");
            break;
        }
    }
    close(sock);
    return 0;
}

//显示认证菜单
void display_auth_menu(void)
{
    printf("\n=== 认证选项 ===\n");
    printf("1. 注册\n");
    printf("2. 登录\n");
    printf("3. 直接连接\n");
    printf("================\n");
    printf("请选择操作: ");
}

//发送认证请求
void send_auth_request(int sock, const char* username, const char* password, const char* cmd)
{
    char request[512];
    snprintf(request, sizeof(request), "%s %s %s", cmd, username, password);
    send(sock, request, strlen(request), 0);
}

//处理认证响应
int handle_auth_response(int sock)
{
    char auth_resp[1024];
    int rec = recv(sock, auth_resp, sizeof(auth_resp) - 1, 0);
    auth_resp[rec] = '\0';

    if (rec <= 0) {
        fprintf(stderr, "连接关闭\n");
        return -3;
    }

    if (strncmp(auth_resp, AUTH_REQUIRED, 13) == 0) {
        // 需要认证
        char username[50] = {0};
        char password[50] = {0};
        int auth_option = 0;

        display_auth_menu();
        scanf("%d", &auth_option);
        getchar(); // 清除输入缓冲区

        switch (auth_option) {
            case AUTH_OPTION_REGISTER:
                printf("请输入用户名: ");
                fgets(username, sizeof(username), stdin);
                username[strcspn(username, "\n")] = '\0';

                printf("请输入密码: ");
                fgets(password, sizeof(password), stdin);
                password[strcspn(password, "\n")] = '\0';

                send_auth_request(sock, username, password, REGISTER_CMD);
                break;

            case AUTH_OPTION_LOGIN:
                printf("请输入用户名: ");
                fgets(username, sizeof(username), stdin);
                username[strcspn(username, "\n")] = '\0';

                printf("请输入密码: ");
                fgets(password, sizeof(password), stdin);
                password[strcspn(password, "\n")] = '\0';

                send_auth_request(sock, username, password, LOGIN_CMD);
                break;

            case AUTH_OPTION_DIRECT_CONNECT:
                send_auth_request(sock, "", "", "DIRECT");
                break;

            default:
                fprintf(stderr, "无效的选项\n");
                return -3;
        }

        // 等待认证响应
        rec = recv(sock, auth_resp, sizeof(auth_resp) - 1, 0);
        auth_resp[rec] = '\0';

        if (rec <= 0) {
            fprintf(stderr, "连接关闭\n");
            return -3;
        }

        if (strncmp(auth_resp, REGISTER_OK, 11) == 0) {
            printf("注册成功!\n");
            return 1;
        } else if (strncmp(auth_resp, REGISTER_FAIL, 17) == 0) {
            printf("注册失败: %s\n", auth_resp + 17);
            return -1;
        } else if (strncmp(auth_resp, LOGIN_OK, 8) == 0) {
            printf("登录成功!\n");
            return 1;
        } else if (strncmp(auth_resp, LOGIN_FAIL, 14) == 0) {
            printf("登录失败: %s\n", auth_resp + 14);
            return -2;
        } else if (strncmp(auth_resp, AUTH_REQUIRED, 13) == 0) {
            // 其他情况，可能是直接连接
            printf("直接连接模式\n");
            return 1;
        } else {
            fprintf(stderr, "未知的认证响应: %s\n", auth_resp);
            return -3;
        }
    } else {
        // 服务器没有要求认证
        printf("直接连接模式\n");
        return 1;
    }
}

int recve(int sock, file_inof_t* file, char* buff)
{
    // 设置超时，等待文件信息 5秒
    struct timeval tv;
    tv.tv_sec = 5;   
    tv.tv_usec = 0;
    setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));

    int rec;
    rec = recv(sock, file, sizeof(file_inof_t), 0);

    if(rec == -1 && errno == EAGAIN) 
    {
        // 超时，没有新文件了
        return -2;
    }
    if(rec != sizeof(file_inof_t))
    {
        perror("recv fail");
        exit(1);
    }

    //解除超时设置，准备接收文件数据
    tv.tv_sec = 0;
    tv.tv_usec = 0;
    setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));

    struct stat st;
    rcv_file.file = *file;
    rcv_file.received_size = read_resume(file->name); //读取断点续传信息
    if(stat(file->name, &st) == 0)
    {
        if(st.st_size >= file->size) 
        {
            printf("文件已完整，跳过下载\n");
            return 0;
        }
        // 以实际文件大小为准，忽略进度文件（如果不同）
        if(st.st_size != rcv_file.received_size) 
        {
            printf("进度记录与实际文件大小不符，以实际为准\n");
            rcv_file.received_size = st.st_size;
            save_resume(file->name, rcv_file.received_size);
        }
        printf("续传模式，从 %zu 字节开始\n", rcv_file.received_size);
    } 
    else 
    {
        rcv_file.received_size = 0;
        printf("新文件传输\n");
    }

    //发送续传请求
    char resume_quest[512];
    snprintf(resume_quest, sizeof(resume_quest), "RESUME %zu", rcv_file.received_size);
    send(sock, resume_quest, strlen(resume_quest), 0);

    //接收服务端的续传确认
    int len = recv(sock, buff, buff_size-1, 0);
    buff[len] = '\0';
    if(len <= 0 || strncmp(buff, "OK", 2) != 0) 
    {
        printf("服务端不支持续传，从头开始\n");
        rcv_file.received_size = 0;
    }

    int flags = O_WRONLY | O_CREAT;
    if(rcv_file.received_size == 0) flags |= O_TRUNC;
    int fd = open(file->name, flags, 0644);
    if(fd == -1)
    {
        perror("open fail");
        return -1;
    }

    //开始接收文件数据
    size_t last_save = rcv_file.received_size;
    size_t save_interval = file->size / 20;  // 每5%保存一次
    if(save_interval < 1024) save_interval = 1024;
    lseek(fd, rcv_file.received_size, SEEK_SET); //移动文件指针到续传位置

    while(rcv_file.received_size < file->size)
    {
        // 计算本次应该接收的字节数
        size_t to_recv = buff_size;
        if(to_recv > file->size - rcv_file.received_size)
        {
            to_recv = file->size - rcv_file.received_size;
        }
        
        rec = recv(sock, buff, to_recv, 0);
        if(rec <= 0)  // 连接关闭或错误
        {
            if(rec == 0)
                fprintf(stderr, "Connection closed prematurely\n");
            else
                perror("recv fail");
            close(fd);
            save_resume(rcv_file.file.name, rcv_file.received_size);
            return -1;
        }
        
        // 写入文件（只写入实际接收的字节数）
        int write_rec = write(fd, buff, rec);
        if(write_rec != rec)
        {
            perror("write fail");
            close(fd);
            save_resume(rcv_file.file.name, rcv_file.received_size);
            return -1;
        }
        
        //计算已接收的数据，并保存端点续传信息
        rcv_file.received_size += rec;
        if(rcv_file.received_size - last_save >= save_interval)
        {
        save_resume(rcv_file.file.name, rcv_file.received_size);
        last_save = rcv_file.received_size;
        }
        printf("Progress: %.1f%%\r", (double)rcv_file.received_size / file->size * 100);
        fflush(stdout);
    }
    
    printf("\n");
    close(fd);
    return 0;
}

