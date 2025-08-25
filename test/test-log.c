#include "test_helper.h"
#include "../src/component/log.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void test_log_basic() {
    // 测试基本日志功能
    log_init(LOG_LEVEL_DEBUG, LOG_TARGET_CONSOLE, NULL);
    
    LOG_DEBUG("test", "This is a debug message");
    LOG_INFO("test", "This is an info message");
    LOG_WARN("test", "This is a warning message");
    LOG_ERROR("test", "This is an error message");
    
    // 测试不同模块标签
    LOG_INFO("network", "Network module initialized");
    LOG_WARN("storage", "Low disk space warning");
    LOG_ERROR("database", "Failed to connect to database");
}

void test_log_levels() {
    // 测试不同日志级别
    printf("\n=== Testing INFO level ===\n");
    log_init(LOG_LEVEL_INFO, LOG_TARGET_CONSOLE, NULL);
    
    LOG_DEBUG("test", "This debug message should NOT appear");
    LOG_INFO("test", "This info message SHOULD appear");
    LOG_WARN("test", "This warning message SHOULD appear");
    LOG_ERROR("test", "This error message SHOULD appear");
    
    printf("\n=== Testing ERROR level ===\n");
    log_init(LOG_LEVEL_ERROR, LOG_TARGET_CONSOLE, NULL);
    
    LOG_DEBUG("test", "This debug message should NOT appear");
    LOG_INFO("test", "This info message should NOT appear");
    LOG_WARN("test", "This warning message should NOT appear");
    LOG_ERROR("test", "This error message SHOULD appear");
}

int main() {
    test_log_basic();
    test_log_levels();
    log_destroy();
    return 0;
}