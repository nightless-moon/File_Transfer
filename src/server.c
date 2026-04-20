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
    //定义ip信息
    struct sockaddr_in sockaddr;
    sockaddr.sin_family = AF_INET;             //家族
    sockaddr.sin_addr.s_addr = INADDR_ANY;     //任意主机号
    sockaddr.sin_port = htons(9999);           //端口号9999

    //绑定地址
    if(bind(sock_listen, (struct sockaddr*)&sockaddr, sizeof(sockaddr)) == -1)
    {
        perror("bind fail");
        exit(2);
    }

    //设置监听
    if(listen(sock_listen, 5) == -1)
    {
        perror("listen fail");
        exit(3);
    }

    //接收客户端请求
    int sock_conn;
    pthread_t tid;
    struct sockaddr_in  user_addr;
    socklen_t addr_len = sizeof(user_addr);
    user* user_xin = NULL;
    while(1)
    {
        sock_conn = accept(sock_listen, (struct sockaddr*)&user_addr, &addr_len);
        if(sock_conn == -1)
        {
            perror("accept fail");
            exit(4);
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
        user_xin->port = htnos(user_addr.sin_port);
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

    }

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
        
    }
     
}

//向特定客户端发送文件
int send(int sock, const char* file_path)
{
    char msg[1024];
    int fd = open(file_path, O_RDONLY);
    if(fd == -1)
    {
        perror("open fail");
        exit(1);
    }

    write(sock, msg, sizeof(msg));

    return 0;
}

