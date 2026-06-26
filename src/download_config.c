#include "download.h"
#include <stdio.h>
#include <stdlib.h>

// 文件大小与线程数的映射关系
#define MIN_BLOCK_SIZE 64 * 1024    // 64KB
#define MAX_THREADS_SINGLE_FILE 16   // 单文件最大线程数

// 计算下载线程数
int calculate_thread_count(uint64_t file_size) {
    if (file_size < 5 * 1024 * 1024) {
        return 1;  // 单线程
    } else if (file_size < 100 * 1024 * 1024) {
        return file_size < 20 * 1024 * 1024 ? 2 : 4;
    } else if (file_size < 1024 * 1024 * 1024) {
        return 4 + (file_size / 100 * 1024 * 1024) / 10;
    } else {
        return 8 + (file_size / 1024 * 1024 * 1024) / 100;
    }
}

// 计算块大小
uint64_t calculate_block_size(uint64_t file_size, int thread_count) {
    if (thread_count <= 0) {
        thread_count = 1;
    }

    uint64_t block_size = file_size / thread_count;

    // 确保块大小至少为64KB
    if (block_size < MIN_BLOCK_SIZE) {
        block_size = MIN_BLOCK_SIZE;
    }

    // 限制最大块大小
    if (block_size > 1 * 1024 * 1024) {  // 1MB
        block_size = 1 * 1024 * 1024;
    }

    return block_size;
}

// 初始化下载配置
int init_download_config(download_config_t *config, uint64_t file_size) {
    if (!config) {
        return DOWNLOAD_ERROR_UNKNOWN;
    }

    if (file_size == 0) {
        config->thread_count = 1;
        config->block_size = MIN_BLOCK_SIZE;
        config->chunk_size = buff_size;
        config->use_multi_thread = 0;
        return DOWNLOAD_SUCCESS;
    }

    // 计算线程数
    config->thread_count = calculate_thread_count(file_size);

    // 计算块大小
    config->block_size = calculate_block_size(file_size, config->thread_count);

    // 设置数据块大小（通常使用缓冲区大小）
    config->chunk_size = buff_size;

    // 判断是否启用多线程
    config->use_multi_thread = (config->thread_count > 1);

    return DOWNLOAD_SUCCESS;
}

// 销毁下载配置
void destroy_download_config(download_config_t *config) {
    if (!config) {
        return;
    }
    // 配置结构体不需要释放内存
}

// 获取下载配置信息（用于调试）
void print_download_config(download_config_t *config) {
    if (!config) {
        return;
    }

    printf("下载配置:\n");
    printf("  文件大小: %zu bytes\n", config->block_size * config->thread_count);
    printf("  线程数: %zu\n", config->thread_count);
    printf("  块大小: %zu bytes\n", config->block_size);
    printf("  数据块大小: %zu bytes\n", config->chunk_size);
    printf("  启用多线程: %s\n", config->use_multi_thread ? "是" : "否");
}
