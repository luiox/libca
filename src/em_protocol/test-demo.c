// 这个文件用于模拟如何使用
#include "file_transfer.h"
#include "xmodem.h"
#include <stdio.h>

// --- 模拟硬件底层接口 ---

// 模拟串口发送
void uart_send(const uint8_t* data, size_t length) {
    printf("[UART SEND] ");
    for (size_t i = 0; i < length; i++) {
        printf("%02X ", data[i]);
    }
    printf("\n");
}

// 模拟 Flash 写入
void write_flash(uint32_t address, const uint8_t* data, size_t length) {
    printf("[FLASH WRITE] Addr: 0x%08X, Len: %zu\n", address, length);
}

// --- 协议相关全局变量 ---

file_transfer_t g_ft; // 统一的协议句柄
xmodem_t g_xm;        // XModem 私有数据
ringbuffer_t g_rb;
ringbuffer_t g_sb;

// 为环形缓冲区分配实际内存
uint8_t rb_mem[1024];
uint8_t sb_mem[128];

// --- 回调函数实现 ---

void on_data_received(const u8* data, usize len, usize offset) {
    // 协议解析出一块完整数据后，会调用这里
    write_flash(0x08020000 + offset, data, len);
}

// --- 辅助函数：处理协议生成的回复 ---
void handle_protocol_reply(void) {
    uint8_t tx_buf[128];
    // 通过统一接口 ft->ops->poll 从协议实例中“拉”出待发送的数据
    i32 len = g_ft.ops->poll(g_ft.proto_ins, tx_buf, sizeof(tx_buf));
    if (len > 0) {
        uart_send(tx_buf, (size_t)len);
    }
}

// --- 模拟中断或数据接收 ---
void on_uart_rx_event(const uint8_t* data, size_t len) {
    // 仅仅将数据存入接收缓冲区，不进行任何协议处理，保证中断极速退出
    ringbuffer_write(&g_rb, data, len);
}

int main() {
    // 1. 初始化环形缓冲区内存
    ringbuffer_init(&g_rb, rb_mem, sizeof(rb_mem));
    ringbuffer_init(&g_sb, sb_mem, sizeof(sb_mem));

    // 2. 初始化协议私有数据
    xmodem_proto_init(&g_xm, &g_rb, &g_sb);
    xmodem_set_on_data_cb(&g_xm, on_data_received);

    // 3. 初始化统一协议接口，关联具体的协议实例
    file_transfer_init(&g_ft, TP_XMODEM, &g_xm);

    printf("File Transfer Demo Started (Protocol: %d)...\n", g_ft.proto);

    // 4. 主循环
    uint32_t last_tick = 0; 
    while (1) {
        // A. 核心处理：通过统一接口驱动协议解析
        // 传入 NULL, 0 表示让协议去处理已经在 g_rb 缓冲区里的数据
        if (g_ft.ops->process(g_ft.proto_ins, NULL, 0) > 0) {
            handle_protocol_reply();
        }

        // B. 时间驱动：处理超时逻辑
        uint32_t current_time = 0; // 模拟时间戳
        if (current_time - last_tick >= 100) {
            if (g_ft.ops->tick(g_ft.proto_ins, 100) > 0) {
                handle_protocol_reply();
            }
            last_tick = current_time;
        }
    }

    return 0;
}
