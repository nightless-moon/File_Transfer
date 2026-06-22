#ifndef AUTH_H
#define AUTH_H

#include <stddef.h>

// 认证消息类型
#define AUTH_REQUIRED "AUTH_REQUIRED"
#define REGISTER_CMD "REGISTER"
#define LOGIN_CMD "LOGIN"
#define REGISTER_OK "REGISTER_OK"
#define REGISTER_FAIL "REGISTER_FAIL"
#define LOGIN_OK "LOGIN_OK"
#define LOGIN_FAIL "LOGIN_FAIL"

// 认证选项
#define AUTH_OPTION_REGISTER 1
#define AUTH_OPTION_LOGIN 2
#define AUTH_OPTION_DIRECT_CONNECT 3

// 认证超时时间（秒）
#define AUTH_TIMEOUT 5

// 函数声明
void send_auth_request(int sock, const char* username, const char* password, const char* cmd);
int handle_auth_response(int sock);
void display_auth_menu(void);

#endif
