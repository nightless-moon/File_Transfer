#include "server.h"
#include "user.h"
#include <ctype.h>

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
    sockaddr.sin_family = AF_INET;                    //家族
    sockaddr.sin_addr.s_addr = htonl(INADDR_ANY);     //任意主机号
    sockaddr.sin_port = htons(9999);                  //端口号9999

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

    //用户数据库路径（默认users.db）
    const char* user_db_path = "users.db";
    if (argc >= 2) {
        user_db_path = argv[1];
    }

    // 初始化用户数据库
    if (init_user_db(user_db_path) != 0) {
        perror("init_user_db fail");
        exit(1);
    }
    printf("用户数据库初始化成功: %s\n", user_db_path);

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
    close_user_db();
    return 0;
}


void* comm_thr(void* arg)
{
    int i;
    user* user_xin = (user*)arg;
    pthread_detach(pthread_self());
    printf("\n客户端(%s:%hu)上线!\n", user_xin->ip, user_xin->port);

    // 认证阶段
    char auth_resp[1024];
    int rec = recv(user_xin->sock_conn, auth_resp, sizeof(auth_resp) - 1, 0);
    auth_resp[rec] = '\0';

    if (rec <= 0) {
        fprintf(stderr, "客户端认证失败: 连接关闭\n");
        free(user_xin);
        close(user_xin->sock_conn);
        return NULL;
    }

    if (strncmp(auth_resp, "REGISTER", 8) == 0) {
        // 注册请求
        char username[50] = {0};
        char password[50] = {0};

        // 解析用户名和密码
        char* space1 = strchr(auth_resp, ' ');
        if (!space1) {
            send(user_xin->sock_conn, "REGISTER_FAIL:格式错误", 26, 0);
            close(user_xin->sock_conn);
            free(user_xin);
            return NULL;
        }
        space1++;

        char* space2 = strchr(space1, ' ');
        if (!space2) {
            send(user_xin->sock_conn, "REGISTER_FAIL:格式错误", 26, 0);
            close(user_xin->sock_conn);
            free(user_xin);
            return NULL;
        }
        space2++;

        int len = space2 - space1;
        if (len >= 49) len = 49;
        strncpy(username, space1, len);
        username[len] = '\0';

        len = strlen(space2);
        if (len >= 49) len = 49;
        strncpy(password, space2, len);
        password[len] = '\0';

        printf("收到注册请求: %s\n", username);

        int result = register_user(username, password, user_xin->ip, user_xin->port);
        if (result == 0) {
            save_users_to_file("users.json");
            printf("用户 %s 注册成功\n", username);
            send(user_xin->sock_conn, "REGISTER_OK", 11, 0);
            strcpy(user_xin->username, username);
            user_xin->is_authenticated = 1;
        } else {
            printf("用户 %s 注册失败\n", username);
            const char* msg = result == -1 ? "用户已存在" : "用户列表已满";
            send(user_xin->sock_conn, msg, strlen(msg), 0);
            close(user_xin->sock_conn);
            free(user_xin);
            return NULL;
        }

    } else if (strncmp(auth_resp, "LOGIN", 5) == 0) {
        // 登录请求
        char username[50] = {0};
        char password[50] = {0};

        // 解析用户名和密码
        char* space1 = strchr(auth_resp, ' ');
        if (!space1) {
            send(user_xin->sock_conn, "LOGIN_FAIL:格式错误", 21, 0);
            close(user_xin->sock_conn);
            free(user_xin);
            return NULL;
        }
        space1++;

        char* space2 = strchr(space1, ' ');
        if (!space2) {
            send(user_xin->sock_conn, "LOGIN_FAIL:格式错误", 21, 0);
            close(user_xin->sock_conn);
            free(user_xin);
            return NULL;
        }
        space2++;

        int len = space2 - space1;
        if (len >= 49) len = 49;
        strncpy(username, space1, len);
        username[len] = '\0';

        len = strlen(space2);
        if (len >= 49) len = 49;
        strncpy(password, space2, len);
        password[len] = '\0';

        printf("收到登录请求: %s\n", username);

        int result = login_user(username, password);
        if (result == 0) {
            printf("用户 %s 登录成功\n", username);
            send(user_xin->sock_conn, "LOGIN_OK", 8, 0);
            strcpy(user_xin->username, username);
            user_xin->is_authenticated = 1;
        } else {
            printf("用户 %s 登录失败\n", username);
            const char* msg = result == -1 ? "用户不存在" : "密码错误";
            send(user_xin->sock_conn, msg, strlen(msg), 0);
            close(user_xin->sock_conn);
            free(user_xin);
            return NULL;
        }

    } else {
        // 其他情况，要求先认证
        send(user_xin->sock_conn, "AUTH_REQUIRED", 13, 0);
        // 关闭连接，客户端应该重新连接
        close(user_xin->sock_conn);
        free(user_xin);
        return NULL;
    }

    // 认证通过后，发送文件
    // 先发送多线程下载选项协商
    printf("向客户端(%s:%hu)发送多线程下载选项\n", user_xin->ip, user_xin->port);

    for(i = 0; i < user_xin->send_file_cnt; i++)
    {
        // 检查是否为文件夹
        struct stat st;
        if(lstat(user_xin->send_file_list[i], &st) == 0 && S_ISDIR(st.st_mode)) {
            if(send_folder(user_xin->sock_conn, user_xin->send_file_list[i]) == 0)
            {
                printf("向客户端(%s:%hu)发送文件夹%s成功!\n", user_xin->ip, user_xin->port, user_xin->send_file_list[i]);
            }
            else
            {
                printf("向客户端(%s:%hu)发送文件夹%s失败!\n", user_xin->ip, user_xin->port, user_xin->send_file_list[i]);
            }
        } else {
            if(send_file(user_xin->sock_conn, user_xin->send_file_list[i]) == 0)
            {
                printf("向客户端(%s:%hu)发送文件%s成功!\n", user_xin->ip, user_xin->port, user_xin->send_file_list[i]);
            }
            else
            {
                printf("向客户端(%s:%hu)发送文件%s失败!\n", user_xin->ip, user_xin->port, user_xin->send_file_list[i]);
            }
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

    //等待客户端的续传请求

     int fd = open(file_path, O_RDONLY);
    if(fd == -1)
    {
        perror("open fail");
        return 3;
    }

    char buff[512];
    rec = recv(sock, buff, sizeof(buff)-1, 0);
    if(rec <= 0)    
    {
        if(rec == 0)
            fprintf(stderr, "Connection closed by client\n");
        else
            perror("recv fail");
        return 3;
    }
    buff[rec] = '\0';
    if(strncmp(buff, "RESUME", 6) == 0)
    {
        size_t offset = strtoul(buff + 7, NULL, 10);
        if(offset < fi.size)
        {
            // 客户端请求续传，发送确认
            send(sock, "OK", 2, 0);
            // 将文件指针移动到续传位置
            lseek(fd, offset, SEEK_SET);
            send_cnt = offset; // 更新已发送字节数
        }
        else
        {
            // 客户端请求续传，但偏移量无效，拒绝续传
            send(sock, "NO", 2, 0);
            lseek(fd, 0, SEEK_SET); // 从头开始发送
        }
    }
    else
    {
        // 客户端未请求续传，默认从头开始发送
        lseek(fd, 0, SEEK_SET);
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

// 扫描文件夹，获取所有文件和子文件夹信息
typedef struct {
    char path[512];                    // 完整路径
    char name[101];                    // 文件名或文件夹名
    int is_dir;                        // 是否为文件夹
    uint64_t size;                     // 文件大小（文件夹为0）
} file_scan_item_t;

static int scan_directory(const char* dir_path, file_scan_item_t** items, int* item_count)
{
    DIR *dir;
    struct dirent *entry;
    file_scan_item_t *local_items = NULL;
    int local_count = 0;

    dir = opendir(dir_path);
    if (dir == NULL) {
        perror("opendir fail");
        return -1;
    }

    while ((entry = readdir(dir)) != NULL) {
        // 跳过 . 和 ..
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
            continue;
        }

        char full_path[512];
        snprintf(full_path, sizeof(full_path), "%s/%s", dir_path, entry->d_name);

        struct stat st;
        if (lstat(full_path, &st) == -1) {
            perror("lstat fail");
            continue;
        }

        // 添加到列表
        local_items = realloc(local_items, (local_count + 1) * sizeof(file_scan_item_t));
        if (local_items == NULL) {
            perror("realloc fail");
            closedir(dir);
            return -1;
        }

        strncpy(local_items[local_count].path, full_path, sizeof(local_items[local_count].path) - 1);
        local_items[local_count].path[sizeof(local_items[local_count].path) - 1] = '\0';

        strncpy(local_items[local_count].name, entry->d_name, sizeof(local_items[local_count].name) - 1);
        local_items[local_count].name[sizeof(local_items[local_count].name) - 1] = '\0';

        local_items[local_count].is_dir = S_ISDIR(st.st_mode);
        local_items[local_count].size = (local_items[local_count].is_dir) ? 0 : st.st_size;

        local_count++;
    }

    closedir(dir);

    *items = local_items;
    *item_count = local_count;
    return 0;
}

// 递归扫描文件夹并发送
int send_directory_recursive(int sock, const char* dir_path, const char* relative_path)
{
    file_scan_item_t *items = NULL;
    int item_count = 0;

    // 扫描目录
    if (scan_directory(dir_path, &items, &item_count) != 0) {
        return -1;
    }

    // 先发送文件夹信息（is_dir=1）
    for (int i = 0; i < item_count; i++) {
        if (items[i].is_dir) {
            file_info fi = {0};
            strncpy(fi.name, items[i].name, sizeof(fi.name) - 1);
            fi.name[sizeof(fi.name) - 1] = '\0';
            fi.size = 0;  // 文件夹大小为0
            fi.mode = 0;

            if (write(sock, &fi, sizeof(fi)) != sizeof(fi)) {
                perror("write folder info fail");
                free(items);
                return -1;
            }
        }
    }

    // 然后发送文件和递归处理子文件夹
    for (int i = 0; i < item_count; i++) {
        if (items[i].is_dir) {
            // 创建子文件夹路径
            char sub_dir_path[512];
            if (relative_path == NULL || strlen(relative_path) == 0) {
                snprintf(sub_dir_path, sizeof(sub_dir_path), "%s", items[i].path);
            } else {
                snprintf(sub_dir_path, sizeof(sub_dir_path), "%s/%s", relative_path, items[i].name);
            }

            // 递归处理子文件夹
            if (send_directory_recursive(sock, items[i].path, sub_dir_path) != 0) {
                free(items);
                return -1;
            }
        } else {
            // 发送文件
            char file_path[512];
            if (relative_path == NULL || strlen(relative_path) == 0) {
                strncpy(file_path, items[i].path, sizeof(file_path) - 1);
            } else {
                snprintf(file_path, sizeof(file_path), "%s/%s", relative_path, items[i].name);
            }

            if (send_file(sock, file_path) != 0) {
                perror("send file fail");
                free(items);
                return -1;
            }
        }
    }

    free(items);
    return 0;
}

// 向特定客户端发送文件夹
int send_folder(int sock, const char* folder_path)
{
    printf("正在扫描文件夹: %s\n", folder_path);

    // 先发送文件夹信息（特殊标记）
    file_info fi = {0};
    strncpy(fi.name, folder_path, sizeof(fi.name) - 1);
    fi.name[sizeof(fi.name) - 1] = '\0';
    fi.size = 0;
    fi.mode = 0;

    if (write(sock, &fi, sizeof(fi)) != sizeof(fi)) {
        perror("write folder info fail");
        return 1;
    }

    // 递归发送文件夹内容
    if (send_directory_recursive(sock, folder_path, NULL) != 0) {
        perror("send directory fail");
        return 2;
    }

    printf("文件夹发送完成\n");
    return 0;
}

// 分块发送文件
int send_file_chunk(int sock, const char* file_path, uint64_t offset, uint64_t size) {
    char msg[1024];
    struct stat st;
    const char* file_name = NULL;

    // 获取文件信息
    if(lstat(file_path, &st) == -1) {
        perror("stat fail");
        return 1;
    }

    // 验证偏移量
    if(offset >= st.st_size) {
        return 2;
    }

    // 计算实际要发送的大小
    uint64_t actual_size = size;
    if(offset + size > st.st_size) {
        actual_size = st.st_size - offset;
    }

    // 打开文件
    int fd = open(file_path, O_RDONLY);
    if(fd == -1) {
        perror("open fail");
        return 3;
    }

    // 移动文件指针到偏移量位置
    lseek(fd, offset, SEEK_SET);

    // 发送数据块
    int rec;
    char buff[512];
    uint64_t sent_cnt = 0;

    while(sent_cnt < actual_size) {
        rec = read(fd, buff, sizeof(buff));
        if(rec <= 0) {
            perror("read fail");
            close(fd);
            return 4;
        }

        if(write(sock, buff, rec) != rec) {
            perror("write_file fail");
            close(fd);
            return 5;
        }

        sent_cnt += rec;
    }

    close(fd);
    return 0;
}


