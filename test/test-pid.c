#include <stdio.h>
#include <locale.h>
#include "util/pid.h"

void test_pid_position() {
    pid_position_t pid;
    pid_position_init(&pid, 1.0f, 0.1f, 0.05f, 10.0f); // 目标10
    float values[] = {0, 2, 5, 8, 9, 10, 11, 12};
    printf("[位置式PID]\n");
    for (int i = 0; i < sizeof(values)/sizeof(values[0]); ++i) {
        float u = pid_position_calculate(&pid, values[i]);
        printf("输入:%.2f, 输出:%.2f\n", values[i], u);
    }
}

void test_pid_incremental() {
    pid_incremental_t pid;
    pid_incremental_init(&pid, 1.0f, 0.1f, 0.05f, 10.0f);
    float values[] = {0, 2, 5, 8, 9, 10, 11, 12};
    float u = 0;
    printf("[增量式PID]\n");
    for (int i = 0; i < sizeof(values)/sizeof(values[0]); ++i) {
        float du = pid_incremental_calculate(&pid, values[i]);
        u += du;
        printf("输入:%.2f, 增量:%.2f, 输出:%.2f\n", values[i], du, u);
    }
}

int main()
{
    // 设置控制台编码为UTF-8
    setlocale(LC_ALL, "zh_CN.UTF-8");
    system("chcp 65001"); // Windows下设置控制台为UTF-8编码
    test_pid_position();
    printf("\n");
    test_pid_incremental();
    return 0;
}
