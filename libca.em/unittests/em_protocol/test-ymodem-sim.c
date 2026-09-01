#if TEST_ENABLE
#    include "ymodem.h"
#    include <em_test/test.h>
#    include <em_util/crc.h>
#    include <em_base/memory_util.h>
#    include <em_base/debug.h>
#    include <string.h>
#    include <stdio.h>

// 日志开关：0=关闭 1=摘要 2=全量
#    ifndef YMODEM_SIM_LOG_MODE
#        define YMODEM_SIM_LOG_MODE 1
#    endif

#    define YMODEM_SIM_LOG_SUMMARY 1
#    define YMODEM_SIM_LOG_VERBOSE 2

#    if YMODEM_SIM_LOG_MODE >= YMODEM_SIM_LOG_SUMMARY
#        define LOG_SUMMARY(fmt, ...) printf("[ymodem-sim][summary] " fmt "\n", ##__VA_ARGS__)
#    else
#        define LOG_SUMMARY(fmt, ...) ((void)0)
#    endif

#    if YMODEM_SIM_LOG_MODE >= YMODEM_SIM_LOG_VERBOSE
#        define LOG_VERBOSE(fmt, ...) printf("[ymodem-sim][verbose] " fmt "\n", ##__VA_ARGS__)
#    else
#        define LOG_VERBOSE(fmt, ...) ((void)0)
#    endif

#    define LOG_FAIL(fmt, ...) printf("[ymodem-sim][fail] " fmt "\n", ##__VA_ARGS__)

/* =========================================================================
 * 1. 模拟传输层和通用工具
 * ========================================================================= */

/* YMODEM 协议常量 */
#    define Y_SOH 0x01
#    define Y_STX 0x02
#    define Y_EOT 0x04
#    define Y_ACK 0x06
#    define Y_NAK 0x15
#    define Y_CAN 0x18
#    define Y_CRC 'C'
#    define Y_SUB 0x1A

/* 记录DUT（被测设备）通过io->write发送出去的数据的缓冲区 */
static u8    g_dut_tx_buf[4096];
static usize g_dut_tx_len = 0;

/* 模拟写函数，捕获DUT发送的数据 */
static i32 mock_write(transport_t* self, const u8* buf, usize len)
{
    if (g_dut_tx_len + len <= sizeof(g_dut_tx_buf)) {
        memcpy(g_dut_tx_buf + g_dut_tx_len, buf, len);
        g_dut_tx_len += len;
    }

    if (len > 0) {
        usize count_c   = 0;
        usize count_ack = 0;
        usize count_nak = 0;
        usize count_can = 0;
        usize count_eot = 0;
        for (usize i = 0; i < len; i++) {
            if (buf[i] == Y_CRC) {
                count_c++;
            }
            else if (buf[i] == Y_ACK) {
                count_ack++;
            }
            else if (buf[i] == Y_NAK) {
                count_nak++;
            }
            else if (buf[i] == Y_CAN) {
                count_can++;
            }
            else if (buf[i] == Y_EOT) {
                count_eot++;
            }
        }
        LOG_VERBOSE("dut->sim len=%u C=%u ACK=%u NAK=%u CAN=%u EOT=%u",
                    (unsigned int)len,
                    (unsigned int)count_c,
                    (unsigned int)count_ack,
                    (unsigned int)count_nak,
                    (unsigned int)count_can,
                    (unsigned int)count_eot);
    }
    return (i32)len;
}

/* 模拟读函数（push模式下不使用） */
static i32 mock_read(transport_t* self, u8* buf, usize len, u32 timeout_ms)
{
    return 0;
}

static transport_t g_mock_io = {.write = mock_write, .read = mock_read, .flush = NULL};

/* 清空模拟IO发送缓冲区 */
static void mock_io_clear(void)
{
    g_dut_tx_len = 0;
}

/* 文件接口模拟 */
static u8    g_file_buf[1024 * 10];
static usize g_file_len       = 0;
static bool  g_transfer_done  = false;
static i32   g_transfer_error = 0;
static char  g_recv_filename[128];
static u32   g_recv_size    = 0;
static bool  g_start_called = false;

/* 数据接收回调 */
static i32 on_recv(void* user_data, u32 offset, const u8* data, usize len)
{
    if (g_file_len + len <= sizeof(g_file_buf)) {
        memcpy(g_file_buf + g_file_len, data, len);
        g_file_len += len;
        LOG_VERBOSE("recv offset=%u len=%u total=%u",
                    (unsigned int)offset,
                    (unsigned int)len,
                    (unsigned int)g_file_len);
        return 0;
    }
    LOG_FAIL("recv overflow offset=%u len=%u total=%u",
             (unsigned int)offset,
             (unsigned int)len,
             (unsigned int)g_file_len);
    return -1;
}

#    define TEST_DATA_SIZE 1500
static u8 g_test_data[TEST_DATA_SIZE];

/* 初始化测试数据，使用递增字节填充 */
static void setup_test_data(void)
{
    for (i32 i = 0; i < TEST_DATA_SIZE; i++) {
        g_test_data[i] = (u8)(i & 0xFF);
    }
}

/* 数据发送回调，接收端不使用 */
static i32 on_send(void* user_data, u32 offset, u8* buf, usize len)
{
    return 0;
}

/* 传输开始回调 */
static void on_start(void* user_data, u32 total_size, const char* filename)
{
    g_start_called = true;
    g_recv_size    = total_size;
    mem_zero(g_recv_filename, sizeof(g_recv_filename));
    if (filename != NULL) {
        usize name_len = strlen(filename);
        if (name_len >= sizeof(g_recv_filename)) {
            name_len = sizeof(g_recv_filename) - 1;
        }
        memcpy(g_recv_filename, filename, name_len);
    }
    LOG_SUMMARY("start filename=%s size=%u",
                (filename != NULL) ? filename : "(null)",
                (unsigned int)total_size);
}

/* 传输完成回调 */
static void on_finish(void* user_data, i32 status)
{
    if (status == 0) {
        g_transfer_done = true;
        LOG_SUMMARY("finish ok size=%u", (unsigned int)g_file_len);
    }
    else {
        g_transfer_error = status;
        LOG_FAIL("finish error=%d", (int)status);
    }
}

static file_transfer_cbs_t g_cbs = {
    .on_recv = on_recv, .on_send = on_send, .on_start = on_start, .on_finish = on_finish};

static void test_hw_puts(const char* str)
{
    (void)str;
}

/* 重置测试环境 */
static void reset_test_env(void)
{
    debug_init(test_hw_puts);
    g_dut_tx_len     = 0;
    g_file_len       = 0;
    g_transfer_done  = false;
    g_transfer_error = 0;
    g_start_called   = false;
    g_recv_size      = 0;
    mem_zero(g_file_buf, sizeof(g_file_buf));
    mem_zero(g_recv_filename, sizeof(g_recv_filename));
}

/* ymodem实例的内部缓冲区（ymodem实现所需） */
static u8 g_ym_internal_buf[2048];

/* =========================================================================
 * 2. 对端模拟器（发送方）
 * ========================================================================= */

typedef enum sim_state_enum
{
    SIM_WAIT_C,
    SIM_WAIT_BLOCK0_ACK,
    SIM_WAIT_C_AFTER_ACK,
    SIM_WAIT_DATA_ACK,
    SIM_WAIT_EOT_NAK,
    SIM_WAIT_EOT_ACK,
    SIM_WAIT_END_ACK,
    SIM_DONE
} sim_state_t;

typedef struct sim_peer
{
    const u8*   tx_data;
    usize       tx_total_len;
    usize       offset;
    u8          seq;
    bool        finished;
    sim_state_t state;
} sim_peer_t;

static usize sim_u32_to_str(u32 value, char* out, usize max_len)
{
    char  tmp[11];
    usize idx = 0;

    if (max_len == 0) {
        return 0;
    }

    if (value == 0) {
        out[0] = '0';
        return 1;
    }

    while (value > 0 && idx < sizeof(tmp)) {
        tmp[idx++] = (char)('0' + (value % 10u));
        value /= 10u;
    }

    usize out_len = (idx < max_len) ? idx : max_len;
    for (usize i = 0; i < out_len; i++) {
        out[i] = tmp[idx - 1 - i];
    }

    return out_len;
}

static usize sim_gen_block0(const char* filename, u32 file_size, u8* out)
{
    usize idx  = 0;
    out[idx++] = Y_SOH;
    out[idx++] = 0x00;
    out[idx++] = 0xFF;

    mem_zero(&out[idx], 128);

    if (filename != NULL && filename[0] != '\0') {
        usize name_len = strlen(filename);
        if (name_len >= 127) {
            name_len = 127;
        }
        memcpy(&out[idx], filename, name_len);

        char  size_buf[11];
        usize size_len = sim_u32_to_str(file_size, size_buf, sizeof(size_buf));
        if (size_len > 0) {
            memcpy(&out[idx + name_len + 1], size_buf, size_len);
        }
    }

    idx += 128;

    u16 crc    = crc16_ymodem(&out[3], 128);
    out[idx++] = (crc >> 8) & 0xFF;
    out[idx++] = crc & 0xFF;

    LOG_VERBOSE("tx block0 filename=%s size=%u crc=0x%04X",
                (filename != NULL) ? filename : "(null)",
                (unsigned int)file_size,
                (unsigned int)crc);

    return idx;
}

static usize sim_gen_data_packet(sim_peer_t* sim, u8* out)
{
    usize idx  = 0;
    out[idx++] = Y_STX;
    out[idx++] = sim->seq;
    out[idx++] = ~sim->seq;

    usize remaining = sim->tx_total_len - sim->offset;
    usize copy_len  = remaining > 1024 ? 1024 : remaining;

    memcpy(&out[idx], sim->tx_data + sim->offset, copy_len);
    if (copy_len < 1024) {
        memset(&out[idx + copy_len], Y_SUB, 1024 - copy_len);
    }
    idx += 1024;

    u16 crc    = crc16_ymodem(&out[3], 1024);
    out[idx++] = (crc >> 8) & 0xFF;
    out[idx++] = crc & 0xFF;

    LOG_VERBOSE("tx data seq=%u offset=%u len=%u crc=0x%04X",
                (unsigned int)sim->seq,
                (unsigned int)sim->offset,
                (unsigned int)copy_len,
                (unsigned int)crc);

    return idx;
}

static void sim_init(sim_peer_t* sim, const u8* data, usize len)
{
    mem_zero(sim, sizeof(sim_peer_t));
    sim->tx_data      = data;
    sim->tx_total_len = len;
    sim->seq          = 1;
    sim->state        = SIM_WAIT_C;
    LOG_SUMMARY("sim init len=%u", (unsigned int)len);
}

static usize sim_step(sim_peer_t* sim, const u8* in_buf, usize in_len, u8* out_buf)
{
    usize out_len = 0;
    bool  has_c   = false;
    bool  has_ack = false;
    bool  has_nak = false;

    LOG_VERBOSE("sim step state=%u in_len=%u offset=%u seq=%u",
                (unsigned int)sim->state,
                (unsigned int)in_len,
                (unsigned int)sim->offset,
                (unsigned int)sim->seq);

    for (usize i = 0; i < in_len; i++) {
        if (in_buf[i] == Y_CRC) {
            has_c = true;
        }
        if (in_buf[i] == Y_ACK) {
            has_ack = true;
        }
        if (in_buf[i] == Y_NAK) {
            has_nak = true;
        }
    }

    if (sim->finished) {
        return 0;
    }

    switch (sim->state) {
    case SIM_WAIT_C:
        if (has_c) {
            LOG_SUMMARY("state WAIT_C -> send block0");
            out_len    = sim_gen_block0("test.bin", (u32)sim->tx_total_len, out_buf);
            sim->state = SIM_WAIT_BLOCK0_ACK;
        }
        break;

    case SIM_WAIT_BLOCK0_ACK:
        if (has_nak) {
            LOG_SUMMARY("state WAIT_BLOCK0_ACK -> resend block0");
            out_len = sim_gen_block0("test.bin", (u32)sim->tx_total_len, out_buf);
        }
        if (has_ack && has_c) {
            LOG_SUMMARY("state WAIT_BLOCK0_ACK -> send data (ACK+C)");
            out_len    = sim_gen_data_packet(sim, out_buf);
            sim->state = SIM_WAIT_DATA_ACK;
        }
        else if (has_ack) {
            LOG_SUMMARY("state WAIT_BLOCK0_ACK -> WAIT_C_AFTER_ACK");
            sim->state = SIM_WAIT_C_AFTER_ACK;
        }
        break;

    case SIM_WAIT_C_AFTER_ACK:
        if (has_c) {
            LOG_SUMMARY("state WAIT_C_AFTER_ACK -> send data");
            out_len    = sim_gen_data_packet(sim, out_buf);
            sim->state = SIM_WAIT_DATA_ACK;
        }
        break;

    case SIM_WAIT_DATA_ACK:
        if (has_nak) {
            LOG_SUMMARY("state WAIT_DATA_ACK -> resend data seq=%u", (unsigned int)sim->seq);
            out_len = sim_gen_data_packet(sim, out_buf);
        }
        if (has_ack) {
            sim->offset += 1024;
            sim->seq++;
            if (sim->offset >= sim->tx_total_len) {
                LOG_SUMMARY("state WAIT_DATA_ACK -> send EOT");
                out_buf[out_len++] = Y_EOT;
                sim->state         = SIM_WAIT_EOT_NAK;
            }
            else {
                LOG_SUMMARY("state WAIT_DATA_ACK -> send next data seq=%u", (unsigned int)sim->seq);
                out_len = sim_gen_data_packet(sim, out_buf);
            }
        }
        break;

    case SIM_WAIT_EOT_NAK:
        if (has_nak) {
            LOG_SUMMARY("state WAIT_EOT_NAK -> resend EOT");
            out_buf[out_len++] = Y_EOT;
            sim->state         = SIM_WAIT_EOT_ACK;
        }
        break;

    case SIM_WAIT_EOT_ACK:
        if (has_ack) {
            LOG_SUMMARY("state WAIT_EOT_ACK -> send end block0");
            out_len    = sim_gen_block0("", 0, out_buf);
            sim->state = SIM_WAIT_END_ACK;
        }
        break;

    case SIM_WAIT_END_ACK:
        if (has_nak) {
            LOG_SUMMARY("state WAIT_END_ACK -> resend end block0");
            out_len = sim_gen_block0("", 0, out_buf);
        }
        if (has_ack) {
            LOG_SUMMARY("state WAIT_END_ACK -> done");
            sim->finished = true;
            sim->state    = SIM_DONE;
        }
        break;

    case SIM_DONE:
    default: break;
    }

    return out_len;
}

/* =========================================================================
 * 3. 测试运行循环
 * ========================================================================= */

static void run_protocol_loop(ymodem_t* dut, sim_peer_t* sim)
{
    i32 max_ticks = 2000;
    u8  sim_out_buf[2048];

    while (max_ticks-- > 0 && !sim->finished && !g_transfer_done) {
        ymodem_tick(dut, 100);

        usize sim_out_len = sim_step(sim, g_dut_tx_buf, g_dut_tx_len, sim_out_buf);
        mock_io_clear();

        if (sim_out_len > 0) {
            LOG_VERBOSE("sim->dut len=%u first=0x%02X",
                        (unsigned int)sim_out_len,
                        (unsigned int)sim_out_buf[0]);
            ymodem_process(dut, sim_out_buf, sim_out_len);
        }

        if (g_transfer_error != 0) {
            LOG_FAIL("transfer error=%d", (int)g_transfer_error);
            break;
        }
    }
}

/* =========================================================================
 * 4. 测试用例
 * ========================================================================= */

TEST_CASE(ymodem_rx_1k_basic)
{
    reset_test_env();
    setup_test_data();

    ymodem_t ym;
    mem_zero(&ym, sizeof(ym));
    ymodem_config_t cfg = {.is_transmitter   = false,
                           .recv_buffer      = g_ym_internal_buf,
                           .recv_buffer_size = sizeof(g_ym_internal_buf),
                           .user_data        = NULL,
                           .max_retries      = 10};

    ymodem_init(&ym, &g_mock_io, &g_cbs, &cfg);
    ymodem_start_recv(&ym);

    sim_peer_t sim;
    sim_init(&sim, g_test_data, TEST_DATA_SIZE);

    run_protocol_loop(&ym, &sim);

    TEST_ASSERT_TRUE(g_transfer_done);
    TEST_ASSERT_EQUAL_INT(0, g_transfer_error);
    TEST_ASSERT_TRUE(g_start_called);
    TEST_ASSERT_EQUAL_INT(TEST_DATA_SIZE, (i32)g_file_len);
    TEST_ASSERT_EQUAL_INT(TEST_DATA_SIZE, (i32)g_recv_size);
    TEST_ASSERT_EQUAL_MEMORY(g_test_data, g_file_buf, TEST_DATA_SIZE);
    TEST_ASSERT_EQUAL_INT(0, strcmp(g_recv_filename, "test.bin"));
}
#endif
