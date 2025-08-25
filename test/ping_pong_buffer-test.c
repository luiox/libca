#include <stdio.h>
#include <string.h>
#include "util/ping_pong_buffer.h"

#define BUFFER_SIZE 256

u8 buffer1[BUFFER_SIZE];
u8 buffer2[BUFFER_SIZE];

int main(void)
{
    ping_pong_buffer_t ping_pong_buf;

    // 初始化测试
    printf("Initializing ping-pong buffer...\n");
    ping_pong_buffer_init(&ping_pong_buf, buffer1, buffer2, BUFFER_SIZE);

    printf("Buffer size: %d\n", ping_pong_buffer_get_size(&ping_pong_buf));
    printf("Read buffer address: %p\n", ping_pong_buffer_get_read_buffer(&ping_pong_buf));
    printf("Write buffer address: %p\n", ping_pong_buffer_get_write_buffer(&ping_pong_buf));
    printf("Is writing: %d\n", ping_pong_buffer_is_writing(&ping_pong_buf));

    // 写入一些测试数据到写缓冲区
    u8* write_buf = ping_pong_buffer_get_write_buffer(&ping_pong_buf);
    printf("Starting write operation...\n");
    ping_pong_buffer_start_write(&ping_pong_buf);
    printf("Is writing: %d\n", ping_pong_buffer_is_writing(&ping_pong_buf));

    for (int i = 0; i < 10; i++) {
        write_buf[i] = i + 1;
    }

    printf("Write buffer first 10 values: ");
    for (int i = 0; i < 10; i++) {
        printf("%d ", write_buf[i]);
    }
    printf("\n");

    // 尝试在写入过程中切换缓冲区（应该失败）
    printf("Trying to switch buffers while writing...\n");
    u8 result = ping_pong_buffer_switch(&ping_pong_buf);
    printf("Switch result: %d (0=failed, 1=success)\n", result);

    // 结束写入操作
    printf("Ending write operation...\n");
    ping_pong_buffer_end_write(&ping_pong_buf);
    printf("Is writing: %d\n", ping_pong_buffer_is_writing(&ping_pong_buf));

    // 再次尝试切换缓冲区（应该成功）
    printf("Trying to switch buffers after ending write...\n");
    result = ping_pong_buffer_switch(&ping_pong_buf);
    printf("Switch result: %d (0=failed, 1=success)\n", result);

    printf("After switch:\n");
    printf("Read buffer address: %p\n", ping_pong_buffer_get_read_buffer(&ping_pong_buf));
    printf("Write buffer address: %p\n", ping_pong_buffer_get_write_buffer(&ping_pong_buf));

    // 检查读缓冲区的数据
    u8* read_buf = ping_pong_buffer_get_read_buffer(&ping_pong_buf);
    printf("Read buffer first 10 values: ");
    for (int i = 0; i < 10; i++) {
        printf("%d ", read_buf[i]);
    }
    printf("\n");

    // 清空新的写缓冲区并验证
    u8* new_write_buf = ping_pong_buffer_get_write_buffer(&ping_pong_buf);
    printf("Clearing new write buffer...\n");
    ping_pong_buffer_clear(new_write_buf, BUFFER_SIZE);

    int is_cleared = 1;
    for (int i = 0; i < 10; i++) {
        if (new_write_buf[i] != 0) {
            is_cleared = 0;
            break;
        }
    }

    if (is_cleared) {
        printf("New write buffer successfully cleared\n");
    }
    else {
        printf("Error: New write buffer not cleared\n");
    }

    printf("Ping-pong buffer test completed.\n");
    return 0;
}