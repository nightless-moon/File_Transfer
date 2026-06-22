#include "user.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>

// 全局SQLite数据库连接
static sqlite3* db = NULL;
static user_info_t* user_list = NULL;
static int user_count = 0;
static int max_users = 100; // 最大用户数

// 初始化用户数据库
int init_user_db(const char* db_path)
{
    int rc = sqlite3_open(db_path, &db);
    if (rc) {
        fprintf(stderr, "无法打开数据库: %s\n", sqlite3_errmsg(db));
        return -1;
    }

    // 创建用户表
    const char* sql = "CREATE TABLE IF NOT EXISTS users ("
                      "id INTEGER PRIMARY KEY AUTOINCREMENT,"
                      "username TEXT UNIQUE NOT NULL,"
                      "password TEXT NOT NULL,"
                      "ip TEXT NOT NULL,"
                      "port INTEGER NOT NULL,"
                      "register_time INTEGER NOT NULL);";

    rc = sqlite3_exec(db, sql, NULL, 0, NULL);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "创建用户表失败: %s\n", sqlite3_errmsg(db));
        sqlite3_close(db);
        return -1;
    }

    return 0;
}

// 关闭数据库
int close_user_db(void)
{
    if (db) {
        free(user_list);
        user_list = NULL;
        user_count = 0;
        return sqlite3_close(db);
    }
    return 0;
}

// 查找用户（在内存列表中）
user_info_t* find_user(const char* username)
{
    if (!db || !user_list) {
        return NULL;
    }

    for (int i = 0; i < user_count; i++) {
        if (strcmp(user_list[i].username, username) == 0) {
            return &user_list[i];
        }
    }
    return NULL;
}

// 加载所有用户到内存
static int load_users_to_memory(void)
{
    if (!db) {
        return 0;
    }

    free(user_list);
    user_list = NULL;
    user_count = 0;

    // 查询所有用户
    const char* sql = "SELECT username, password, ip, port, register_time FROM users;";

    sqlite3_stmt* stmt;
    int rc = sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);
    if (rc != SQLITE_OK) {
        return -1;
    }

    // 分配内存
    user_list = malloc(max_users * sizeof(user_info_t));
    if (!user_list) {
        sqlite3_finalize(stmt);
        return -1;
    }

    // 逐行读取
    while (sqlite3_step(stmt) == SQLITE_ROW && user_count < max_users) {
        strncpy(user_list[user_count].username,
                (const char*)sqlite3_column_text(stmt, 0), 49);
        user_list[user_count].username[49] = '\0';

        strncpy(user_list[user_count].password,
                (const char*)sqlite3_column_text(stmt, 1), 49);
        user_list[user_count].password[49] = '\0';

        strncpy(user_list[user_count].ip,
                (const char*)sqlite3_column_text(stmt, 2), 15);
        user_list[user_count].ip[15] = '\0';

        user_list[user_count].port = sqlite3_column_int(stmt, 3);
        user_list[user_count].register_time = sqlite3_column_int64(stmt, 4);

        user_count++;
    }

    sqlite3_finalize(stmt);
    return user_count;
}

// 注册新用户
int register_user(const char* username, const char* password, const char* ip, uint16_t port)
{
    if (!db) {
        return -1;
    }

    // 检查用户是否已存在
    if (find_user(username)) {
        return -1; // 用户已存在
    }

    // 在内存中添加
    if (user_count >= max_users) {
        return -2; // 用户列表已满
    }

    strncpy(user_list[user_count].username, username, 49);
    user_list[user_count].username[49] = '\0';
    strncpy(user_list[user_count].password, password, 49);
    user_list[user_count].password[49] = '\0';
    strncpy(user_list[user_count].ip, ip, 15);
    user_list[user_count].ip[15] = '\0';
    user_list[user_count].port = port;
    user_list[user_count].register_time = time(NULL);

    user_count++;

    // 写入数据库
    const char* sql = "INSERT INTO users (username, password, ip, port, register_time) "
                      "VALUES (?, ?, ?, ?, ?);";

    sqlite3_stmt* stmt;
    int rc = sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);
    if (rc != SQLITE_OK) {
        return -1;
    }

    sqlite3_bind_text(stmt, 1, username, -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 2, password, -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 3, ip, -1, SQLITE_STATIC);
    sqlite3_bind_int(stmt, 4, port);
    sqlite3_bind_int64(stmt, 5, (sqlite3_int64)time(NULL));

    rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    if (rc != SQLITE_DONE) {
        return -1;
    }

    return 0;
}

// 用户登录验证
int login_user(const char* username, const char* password)
{
    user_info_t* user = find_user(username);
    if (!user) {
        return -1; // 用户不存在
    }

    if (strcmp(user->password, password) != 0) {
        return -2; // 密码错误
    }

    return 0; // 登录成功
}

// 显示用户列表
void display_users_list(void)
{
    // 重新加载最新数据
    load_users_to_memory();

    if (user_count == 0) {
        printf("当前没有注册用户\n");
        return;
    }

    printf("\n--- 用户列表 ---\n");
    for (int i = 0; i < user_count; i++) {
        printf("%d. %s (%s:%hu) - 注册时间: %ld\n",
               i + 1,
               user_list[i].username,
               user_list[i].ip,
               user_list[i].port,
               user_list[i].register_time);
    }
    printf("---------------\n");
}
