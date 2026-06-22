#ifndef USER_H
#define USER_H

#include <stdint.h>
#include <time.h>
#include <sqlite3.h>

// 用户信息结构
typedef struct {
    char username[50];
    char password[50];
    char ip[16];
    uint16_t port;
    time_t register_time;
} user_info_t;

// 函数声明
int init_user_db(const char* db_path);
int register_user(const char* username, const char* password, const char* ip, uint16_t port);
int login_user(const char* username, const char* password);
user_info_t* find_user(const char* username);
int close_user_db(void);
void display_users_list(void);

#endif
