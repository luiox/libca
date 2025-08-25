#include "debug.h"
#include "../base/printer.h"
#include <stdio.h>
#include <stdarg.h>
#include <string.h>

// 定义一个缓冲区大小
#define DEBUG_BUFFER_SIZE 256

static uint32_t g_uart;
static printer_t g_debug_printer;
static u8 g_debug_buffer[DEBUG_BUFFER_SIZE];

// 串口发送函数
static void usart_send_buffer(u8* buf, usize size)
{
    // 这里应该实现实际的串口发送功能
    // uart_send(g_uart, buf, size);
    // 作为示例，暂时留空，实际使用时需要实现具体的串口发送逻辑
}

void debug_init(uint32_t uart)
{
    g_uart = uart;
    printer_init(&g_debug_printer, g_debug_buffer, DEBUG_BUFFER_SIZE, usart_send_buffer);
}

// 平台无关，无需修改
void debug_print(const char* fmt, ...)
{
    va_list args;

    va_start(args, fmt);

    // 使用printer_vprintf来输出格式化字符串
    printer_vprintf(&g_debug_printer, fmt, args);

    va_end(args);
}
