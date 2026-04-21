#include "server.h"
int main(int argc, char** argv)
{
    //创建套接字
    int sock_listen = socket(AF_INET, SOCK_STREAM, 0);
    if(-1 == sock_listen)
	{
		perror("socket fail");
		exit(1);
	}

    // 开启地址复用，以允许服务器快速重启
	int val = 1;
	setsockopt(sock_listen, SOL_SOCKET, SO_REUSEADDR, &val, sizeof(val));

    //定义ip信息
    struct sockaddr_in sockaddr;
    sockaddr.sin_family = AF_INET;             //家族
    sockaddr.sin_addr.s_addr = INADDR_ANY;     //任意主机号
    sockaddr.sin_port = htons(9999);           //端口号9999

    //绑定地址
    if(bind(sock_listen, (struct sockaddr*)&sockaddr, sizeof(sockaddr)) == -1)
    {
        perror("bind fail");
        exit(1);
    }

    //设置监听
    if(listen(sock_listen, 5) == -1)
    {
        perror("listen fail");
        exit(1);
    }

    //接收客户端请求
    int sock_conn;
    pthread_t tid;
    struct sockaddr_in  user_addr;
    socklen_t addr_len = sizeof(user_addr);
    user* user_xin = NULL;

    struct timeval tv;
	tv.tv_sec = 10;
	tv.tv_usec = 0;

    while(1)
    {
        sock_conn = accept(sock_listen, (struct sockaddr*)&user_addr, &addr_len);
        if(sock_conn == -1)
        {
            perror("accept fail");
            exit(1);
        }

        user_xin = malloc(sizeof(user));
        if(user_xin == NULL)
        {
            perror("malloc fail");
            close(sock_conn);
			continue;
        }
        //获取客户端信息
        user_xin->sock_conn = sock_conn;
        strcpy(user_xin->ip, inet_ntoa(user_addr.sin_addr));
        user_xin->port = htons(user_addr.sin_port);
        user_xin->online_time = time(NULL);
        user_xin->send_file_list = argv + 1;
        user_xin->send_file_cnt = argc - 1;

        //接收一个客户端后创建一个线程
        if(pthread_create(&tid, NULL, comm_thr, user_xin) == -1)
        {
            perror("pthread_create");
            free(user_xin);
			close(sock_conn);
			continue;
        }
        // 设置接收超时
		setsockopt(sock_conn, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
    }

    free(user_xin);
    close(sock_listen);
    return 0;
}


void* comm_thr(void* arg)
{
    int i;
    user* user_xin = (user*)arg;
    pthread_detach(pthread_self());
    printf("\n客户端(%s:%hu)上线!\n", user_xin->ip, user_xin->port);
    for(i = 0; i < user_xin->send_file_cnt; i++)
    {
        if(send_file(user_xin->sock_conn, user_xin->send_file_list[i]) == 0)
        {
            printf("向客户端(%s:%hu)发送文件%s成功!\n", user_xin->ip, user_xin->port, user_xin->send_file_list[i]);
        }
        else
        {
            printf("向客户端(%s:%hu)发送文件%s失败!\n", user_xin->ip, user_xin->port, user_xin->send_file_list[i]);
        }   
    }
    printf("客户端(%s:%hu)下线!\n", user_xin->ip, user_xin->port);
    return NULL;
}

//向特定客户端发送文件
int send_file(int sock, const char* file_path)
{
    char msg[1024];                     //发送文件内容的缓冲区
    file_info fi = {0};                 //存放文件信息的结构体
    uint64_t send_cnt = 0;              //已发送的字节数
    int rec;                            //接收函数的返回值
    struct stat st;
    const char* file_name = NULL;

    //获取文件信息
    if(lstat(file_path, &st) == -1)
    {
        perror("stat fail");
        return 1;
    }
    fi.size = st.st_size;
    fi.mode = st.st_mode;
    file_name = strrchr(file_path, '/');
    if(file_name == NULL)
    file_name = file_path;
    else
    file_name++;
    strncpy(fi.name, file_name, sizeof(fi.name) - 1);

    //发送文件信息
    if(write(sock, &fi, sizeof(fi))!= sizeof(fi))
    {
        perror("write fail");
        return 2;
    }

    //发送文件内容
    int fd = open(file_path, O_RDONLY);
    if(fd == -1)
    {
        perror("open fail");
        return 3;
    }

    while((rec =read(fd, msg, sizeof(msg))) > 0)
    {
        if(write(sock, msg, rec) != rec)
        {
            perror("write_file fail");
            close(fd);
            return 4;
        }
        send_cnt += rec;
    }
    if(send_cnt != fi.size)
    {
        sprintf(msg, "文件发送失败! 已发送%lu字节, 文件大小%lu字节\n", send_cnt, fi.size);
        close(fd);
        return 5;

    }

    close(fd);
    return 0;
}

