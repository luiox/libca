// 这个文件用于模拟如何使用
#include "file_transfer.h"
#include "xmodem.h"
#include "ymodem.h"
#include <stdio.h>
#include <stdint.h>

// --- 模拟硬件底层接口 ---
// ... (保持不变)

// 模拟串口发送
void uart_send(const uint8_t* data, size_t length)
{
    printf("[UART SEND] ");
    for (size_t i = 0; i < length; i++) {
        printf("%02X ", data[i]);
    }
    printf("\n");
}

// 模拟 Flash 写入
void write_flash(uint32_t address, const uint8_t* data, size_t length)
{
    printf("[FLASH WRITE] Addr: 0x%08X, Len: %zu\n", address, length);
}

// --- 协议相关全局变量 ---

file_transfer_t g_ft;   // 统一的协议句柄
xmodem_t        g_xm;   // XModem 私有数据
ymodem_t        g_ym;   // YModem 私有数据

static u8                  g_xm_buffer[2048];
static u8                  g_ym_buffer[2048];
static xmodem_config_t     g_xm_cfg;
static ymodem_config_t     g_ym_cfg;
static file_transfer_cbs_t g_ft_cbs;

static i32 mock_write(transport_t* self, const u8* buf, usize len)
{
    (void)self;
    uart_send((const uint8_t*)buf, (size_t)len);
    return (i32)len;
}

static i32 mock_read(transport_t* self, u8* buf, usize len, u32 timeout_ms)
{
    (void)self;
    (void)buf;
    (void)len;
    (void)timeout_ms;
    return 0;
}

static transport_t g_mock_io = {.write = mock_write, .read = mock_read, .flush = NULL};

// --- 回调函数实现 ---

static i32 on_data_received(void* user_data, u32 offset, const u8* data, usize len)
{
    (void)user_data;
    // 协议解析出一块完整数据后，会调用这里
    write_flash(0x08020000 + offset, data, len);
    return 0;
}

static void on_ymodem_file_info(void* user_data, u32 total_size, const char* filename)
{
    (void)user_data;
    printf("[YMODEM] Receiving file: %s, size: %u bytes\n", filename, (unsigned int)total_size);
}

int main()
{
    // 2. 选择协议并初始化
    // 切换协议只需要改变这里的 TP_XMODEM 为 TP_YMODEM
    transfer_protocol_enum target_proto = TP_YMODEM;

    g_ft_cbs.on_recv   = on_data_received;
    g_ft_cbs.on_send   = NULL;
    g_ft_cbs.on_start  = on_ymodem_file_info;
    g_ft_cbs.on_finish = NULL;

    if (target_proto == TP_XMODEM) {
        g_xm_cfg.user_data        = NULL;
        g_xm_cfg.is_transmitter   = false;
        g_xm_cfg.mode             = XMODEM_MODE_CRC;
        g_xm_cfg.recv_buffer      = g_xm_buffer;
        g_xm_cfg.recv_buffer_size = sizeof(g_xm_buffer);
        g_xm_cfg.max_retries      = 10;

        xmodem_init(&g_xm, &g_mock_io, &g_ft_cbs, &g_xm_cfg);
        file_transfer_init(&g_ft, TP_XMODEM, &g_xm);
        g_ft.ops->start_recv(g_ft.proto_ins);
    }
    else if (target_proto == TP_YMODEM) {
        g_ym_cfg.user_data        = NULL;
        g_ym_cfg.is_transmitter   = false;
        g_ym_cfg.recv_buffer      = g_ym_buffer;
        g_ym_cfg.recv_buffer_size = sizeof(g_ym_buffer);
        g_ym_cfg.max_retries      = 10;

        ymodem_init(&g_ym, &g_mock_io, &g_ft_cbs, &g_ym_cfg);
        file_transfer_init(&g_ft, TP_YMODEM, &g_ym);
        g_ft.ops->start_recv(g_ft.proto_ins);
    }

    printf("File Transfer Demo Started (Protocol: %d)...\n", g_ft.proto);

    // 4. 主循环
    uint32_t last_tick    = 0;
    uint32_t current_time = 0;   // 模拟时间戳
    u8       rx_buf[256];
    while (1) {
        // 模拟时间流逝
        current_time += 10;

        // A. 数据处理：从底层读取并喂给协议栈
        i32 rx_len = g_mock_io.read(&g_mock_io, rx_buf, sizeof(rx_buf), 0);
        if (rx_len > 0) {
            g_ft.ops->process(g_ft.proto_ins, rx_buf, (usize)rx_len);
        }

        // B. 时间驱动：处理超时逻辑
        if (current_time - last_tick >= 100) {
            g_ft.ops->tick(g_ft.proto_ins, 100);
            last_tick = current_time;
        }
    }

    return 0;
}
