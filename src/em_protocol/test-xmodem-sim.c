#if TEST_ENABLE
#include "xmodem.h"
#include "../em_test/test.h"
#include "../em_util/crc.h"
#include <string.h>
#include <stdio.h>

/* =========================================================================
 * 1. 模拟传输层和通用工具
 * ========================================================================= */

/* XMODEM 协议常量 */
#define X_SOH 0x01
#define X_STX 0x02
#define X_EOT 0x04
#define X_ACK 0x06
#define X_NAK 0x15
#define X_CAN 0x18
#define X_CRC 'C'
#define X_SUB 0x1A
#define X_CTRLZ 0x1A

/* 记录DUT（被测设备）通过io->write发送出去的数据的缓冲区 */
static u8 g_dut_tx_buf[4096]; 
static usize g_dut_tx_len = 0;

/* 模拟写函数，捕获DUT发送的数据 */
static i32 mock_write(transport_t* self, const u8 *buf, usize len) {
    if (g_dut_tx_len + len <= sizeof(g_dut_tx_buf)) {
        memcpy(g_dut_tx_buf + g_dut_tx_len, buf, len);
        g_dut_tx_len += len;
    }
    return len;
}

/* 模拟读函数（push模式下不使用） */
static i32 mock_read(transport_t* self, u8 *buf, usize len, u32 timeout_ms) {
    return 0; /* push模式下不使用 */
}

static transport_t g_mock_io = {
    .write = mock_write,
    .read = mock_read,
    .flush = NULL
};

/* 清空模拟IO发送缓冲区 */
static void mock_io_clear(void) {
    g_dut_tx_len = 0;
}

/* 文件接口模拟 */
static u8 g_file_buf[1024 * 10]; /* 最大10KB测试文件 */
static usize g_file_len = 0;
static bool g_transfer_done = false;
static i32 g_transfer_error = 0;

/* 数据接收回调 */
static i32 on_recv(void *user_data, u32 offset, const u8* data, usize len) {
    if (g_file_len + len <= sizeof(g_file_buf)) {
        memcpy(g_file_buf + g_file_len, data, len);
        g_file_len += len;
        return 0;
    }
    return -1;
}

#define TEST_DATA_SIZE 300
static u8 g_test_data[TEST_DATA_SIZE];

/* 初始化测试数据，使用递增字节填充 */
static void setup_test_data(void) {
    for (i32 i = 0; i < TEST_DATA_SIZE; i++) {
        g_test_data[i] = (u8)(i & 0xFF);
    }
}

/* 数据发送回调，提供要发送的数据 */
static i32 on_send(void *user_data, u32 offset, u8* buf, usize len) {
    if (offset >= TEST_DATA_SIZE) return 0;
    
    usize remain = TEST_DATA_SIZE - offset;
    usize copy_len = (remain < len) ? remain : len;
    
    memcpy(buf, g_test_data + offset, copy_len);
    return (i32)copy_len;
}

/* 传输开始回调（测试中未使用） */
static void on_start(void *user_data, u32 total_size, const char* filename) {
    /* 未使用 */
}

/* 传输完成回调 */
static void on_finish(void *user_data, i32 status) {
    if (status == 0) { // XMODEM_OK
        g_transfer_done = true;
    } else {
        g_transfer_error = status;
    }
}

static file_transfer_cbs_t g_cbs = {
    .on_recv = on_recv,
    .on_send = on_send,
    .on_start = on_start,
    .on_finish = on_finish
};

/* 重置测试环境 */
static void reset_test_env(void) {
    g_dut_tx_len = 0;
    g_file_len = 0;
    g_transfer_done = false;
    g_transfer_error = 0;
    memset(g_file_buf, 0, sizeof(g_file_buf));
}

/* xmodem实例的内部缓冲区（xmodem实现所需） */
static u8 g_xm_internal_buf[2048];

/* =========================================================================
 * 2. 对端模拟器（"另一端"）
 * ========================================================================= */

/* 模拟器角色枚举 */
typedef enum sim_role_enum {
    SIM_ROLE_SENDER,   /* 模拟器作为发送方，DUT 作为接收方 */
    SIM_ROLE_RECEIVER  /* 模拟器作为接收方，DUT 作为发送方 */
} sim_role_t;

/* 模拟器协议类型枚举 */
typedef enum sim_proto_enum {
    SIM_PROTO_CHECKSUM,  /* 标准 XMODEM 校验和 */
    SIM_PROTO_CRC,       /* XMODEM-CRC */
    SIM_PROTO_1K_CRC     /* XMODEM-1K (CRC) */
} sim_proto_t;

/* 模拟器对端结构体 */
typedef struct sim_peer {
    sim_role_t role;       /* 模拟器角色 */
    sim_proto_t proto;     /* 协议类型 */
    
    /* 数据处理 */
    const u8* tx_data;     /* 模拟器要发送的数据 */
    usize tx_total_len;    /* 发送数据总长度 */
    
    u8 rx_buffer[2048];    /* 接收缓冲区（减小到2KB以避免栈溢出风险） */
    usize rx_len;          /* 已接收长度 */

    /* 状态 */
    usize offset;          /* 当前偏移量 */
    u8 seq;                /* 期望的序号(接收)或下一个序号(发送) */
    bool finished;         /* 传输是否完成 */
    
    bool sim_started;      /* 模拟是否已启动 */
    bool handshake_done;   /* 握手是否完成 */
} sim_peer_t;

/**
 * @brief 初始化模拟器对端
 * @param sim 模拟器对端指针
 * @param role 模拟器角色
 * @param proto 协议类型
 * @param data 要发送的数据（仅发送模式使用）
 * @param len 数据长度
 */
static void sim_init(sim_peer_t* sim, sim_role_t role, sim_proto_t proto, const u8* data, usize len) {
    memset(sim, 0, sizeof(sim_peer_t));
    sim->role = role;
    sim->proto = proto;
    sim->tx_data = data;
    sim->tx_total_len = len;
    sim->seq = 1;
}

/**
 * @brief 生成XMODEM数据包
 * @param sim 模拟器对端指针
 * @param out 输出缓冲区
 * @return 生成的数据包长度
 */
static usize sim_gen_packet(sim_peer_t* sim, u8* out) {
    usize idx = 0;
    usize packet_size = (sim->proto == SIM_PROTO_1K_CRC) ? 1024 : 128;
    
    // Header
    out[idx++] = (sim->proto == SIM_PROTO_1K_CRC) ? X_STX : X_SOH;
    out[idx++] = sim->seq;
    out[idx++] = ~sim->seq;
    
    // Body
    usize remaining = sim->tx_total_len - sim->offset;
    usize copy_len = remaining > packet_size ? packet_size : remaining;
    
    memcpy(&out[idx], sim->tx_data + sim->offset, copy_len);
    // Padding
    if (copy_len < packet_size) {
        memset(&out[idx + copy_len], X_SUB, packet_size - copy_len);
    }
    idx += packet_size;
    
    // Checksum/CRC
    if (sim->proto == SIM_PROTO_CHECKSUM) {
        u8 sum = 0;
        for (usize i = 0; i < packet_size; i++) sum += out[3 + i];
        out[idx++] = sum;
    } else {
        u16 crc = crc16_xmodem(&out[3], packet_size);
        out[idx++] = (crc >> 8) & 0xFF;
        out[idx++] = crc & 0xFF;
    }
    
    return idx;
}

/**
 * @brief 模拟对端设备的协议逻辑
 * @param sim 模拟器对端指针
 * @param in_buf DUT发送给模拟器的数据
 * @param in_len 输入数据长度
 * @param out_buf 模拟器要发送给DUT的数据
 * @return 输出数据长度
 */
static usize sim_step(sim_peer_t* sim, const u8* in_buf, usize in_len, u8* out_buf) {
    usize out_len = 0;

    // --- Helper to check input chars ---
    bool has_C = false, has_ACK = false, has_NAK = false, has_EOT = false, has_SOH = false;
    for(usize i=0; i<in_len; i++) {
        if(in_buf[i] == X_CRC) has_C = true;
        if(in_buf[i] == X_ACK) has_ACK = true;
        if(in_buf[i] == X_NAK) has_NAK = true;
        if(in_buf[i] == X_EOT) has_EOT = true;
        if(in_buf[i] == X_SOH) has_SOH = true;
    }

    if (sim->finished) return 0;

    // ==============================================================
    // Logic if Simulator is SENDER (DUT is Receiver)
    // ==============================================================
    if (sim->role == SIM_ROLE_SENDER) {
        if (!sim->handshake_done) {
            // Waiting for start signal
            bool start_condition = false;
            
            if (sim->proto == SIM_PROTO_CHECKSUM) {
                if (has_NAK) start_condition = true;
            } else {
                if (has_C) start_condition = true;
            }

            if (start_condition) {
                sim->handshake_done = true;
                // Send first packet immediately
                out_len = sim_gen_packet(sim, out_buf);
            }
        } else {
            // Already sending
            if (has_ACK) {
                // Determine packet size
                usize packet_size = (sim->proto == SIM_PROTO_1K_CRC) ? 1024 : 128;
                sim->offset += packet_size;
                sim->seq++;

                if (sim->offset >= sim->tx_total_len) {
                    // Done
                    out_buf[out_len++] = X_EOT;
                    if (has_ACK) sim->finished = true;
                } else {
                     out_len = sim_gen_packet(sim, out_buf);
                }
            }
            if (has_NAK) {
                // Resend
                out_len = sim_gen_packet(sim, out_buf);
            }
        }
    } 
    
    // ==============================================================
    // Logic if Simulator is RECEIVER (DUT is Sender)
    // ==============================================================
    else {
        if (!sim->sim_started) {
            sim->sim_started = true;
            if (sim->proto == SIM_PROTO_CHECKSUM) {
                out_buf[out_len++] = X_NAK;
            } else {
                out_buf[out_len++] = X_CRC;
            }
            return out_len;
        }

        // Check if we received a packet
        if (in_len > 0) {
             u8 header = in_buf[0];
             if (header == X_SOH || header == X_STX) {
                 usize data_len = (header == X_STX) ? 1024 : 128;
                 usize overhead = (sim->proto == SIM_PROTO_CHECKSUM) ? 4 : 5;
                 usize total_len = data_len + overhead;
                 
                 if (in_len >= total_len) {
                     u8 seq = in_buf[1];
                     // printf("Sim RX Packet Seq: %d, Len: %zu\n", seq, in_len);
                     u8 seq_inv = in_buf[2];
                     if ((u8)(seq + seq_inv) != 0xFF) {
                         out_buf[out_len++] = X_NAK;
                     } else {
                         // Good packet (Simulating perfect transport)
                         if (sim->rx_len + data_len <= sizeof(sim->rx_buffer)) {
                             memcpy(sim->rx_buffer + sim->rx_len, &in_buf[3], data_len);
                             sim->rx_len += data_len;
                             out_buf[out_len++] = X_ACK;
                         } else {
                             printf("Sim RX buffer overflow!\n");
                             out_buf[out_len++] = X_CAN;
                             sim->finished = true;
                         }
                     }
                 }
             }
             else if (header == X_EOT) {
                 // Check if actually EOT (sometimes it's just 'E' if not careful, but X_EOT is 0x04)
                 out_buf[out_len++] = X_ACK;
                 sim->finished = true;
             }
        }
    }

    return out_len;
}

/* =========================================================================
 * 3. 测试运行循环
 * ========================================================================= */

/**
 * @brief 运行协议循环测试
 * @param dut 被测设备（DUT）的XMODEM实例
 * @param sim 模拟器对端
 */
static void run_protocol_loop(xmodem_t* dut, sim_peer_t* sim) {
    i32 max_ticks = 2000; /* 等效于200秒的最大tick数 */
    u8 sim_out_buf[2048];
    
    printf("Starting loop...\n");
    while (max_ticks-- > 0 && !sim->finished && !g_transfer_done) {
        // 1. Tick DUT
        xmodem_tick(dut, 100);

        // 2. Step Sim
        // (Simulator reads what DUT wrote to g_dut_tx_buf)
        usize sim_out_len = sim_step(sim, g_dut_tx_buf, g_dut_tx_len, sim_out_buf);
        
        // Clear DUT captured buf
        mock_io_clear();

        // 3. Feed Sim output to DUT
        if (sim_out_len > 0) {
            xmodem_process(dut, sim_out_buf, sim_out_len);
        }
        
        // Check Errors
        if (g_transfer_error != 0) {
             printf("Transfer error: %d\n", g_transfer_error);
             break;
        }
    }
    printf("Loop finished. Done=%d, Error=%d\n", g_transfer_done, g_transfer_error);
}

/* =========================================================================
 * 4. 测试用例
 * ========================================================================= */

/* --- 测试组1：DUT作为接收方 --- */

/* 测试用例1：标准XMODEM（校验和） */
TEST_CASE(xmodem_rx_std_checksum) {
    reset_test_env();
    setup_test_data();

    xmodem_t xm;
    memset(&xm, 0, sizeof(xm)); // Ensure clean start
    xmodem_config_t cfg = {
        .is_transmitter = false,
        .mode = XMODEM_MODE_STANDARD, // Forces Checksum
        .recv_buffer = g_xm_internal_buf,
        .recv_buffer_size = sizeof(g_xm_internal_buf),
        .user_data = NULL,
        .max_retries = 10
    };
    xmodem_init(&xm, &g_mock_io, &g_cbs, &cfg);
    xmodem_start_recv(&xm);

    sim_peer_t sim;
    sim_init(&sim, SIM_ROLE_SENDER, SIM_PROTO_CHECKSUM, g_test_data, TEST_DATA_SIZE);

    run_protocol_loop(&xm, &sim);

    TEST_ASSERT_TRUE(g_transfer_done);
    TEST_ASSERT_EQUAL_INT(0, g_transfer_error);
    TEST_ASSERT_EQUAL_INT(384, g_file_len);
    TEST_ASSERT_EQUAL_MEMORY(g_test_data, g_file_buf, TEST_DATA_SIZE);
}

/* 测试用例2：XMODEM CRC */
TEST_CASE(xmodem_rx_crc) {
    reset_test_env();
    setup_test_data();

    xmodem_t xm;
    memset(&xm, 0, sizeof(xm));
    xmodem_config_t cfg = {
        .is_transmitter = false,
        .mode = XMODEM_MODE_CRC,
        .recv_buffer = g_xm_internal_buf,
        .recv_buffer_size = sizeof(g_xm_internal_buf),
        .user_data = NULL,
        .max_retries = 10
    };
    xmodem_init(&xm, &g_mock_io, &g_cbs, &cfg);
    xmodem_start_recv(&xm);

    sim_peer_t sim;
    sim_init(&sim, SIM_ROLE_SENDER, SIM_PROTO_CRC, g_test_data, TEST_DATA_SIZE);

    run_protocol_loop(&xm, &sim);

    TEST_ASSERT_TRUE(g_transfer_done);
    TEST_ASSERT_EQUAL_INT(384, g_file_len); 
    TEST_ASSERT_EQUAL_MEMORY(g_test_data, g_file_buf, TEST_DATA_SIZE);
}

/* 测试用例3：XMODEM 1K */
TEST_CASE(xmodem_rx_1k) {
    reset_test_env();
    setup_test_data();

    xmodem_t xm;
    memset(&xm, 0, sizeof(xm));
    xmodem_config_t cfg = {
        .is_transmitter = false,
        .mode = XMODEM_MODE_1K,
        .recv_buffer = g_xm_internal_buf,
        .recv_buffer_size = sizeof(g_xm_internal_buf),
        .user_data = NULL,
        .max_retries = 10
    };
    xmodem_init(&xm, &g_mock_io, &g_cbs, &cfg);
    xmodem_start_recv(&xm);

    sim_peer_t sim;
    sim_init(&sim, SIM_ROLE_SENDER, SIM_PROTO_1K_CRC, g_test_data, TEST_DATA_SIZE);

    run_protocol_loop(&xm, &sim);

    TEST_ASSERT_TRUE(g_transfer_done);
    TEST_ASSERT_EQUAL_INT(1024, g_file_len);
    TEST_ASSERT_EQUAL_MEMORY(g_test_data, g_file_buf, TEST_DATA_SIZE);
}

/* 测试用例4：XMODEM降级（CRC -> Checksum） */
TEST_CASE(xmodem_rx_fallback) {
    reset_test_env();
    setup_test_data();

    xmodem_t xm;
    memset(&xm, 0, sizeof(xm));
    xmodem_config_t cfg = {
        .is_transmitter = false,
        .mode = XMODEM_MODE_CRC, 
        .recv_buffer = g_xm_internal_buf,
        .recv_buffer_size = sizeof(g_xm_internal_buf),
        .user_data = NULL,
        .max_retries = 10
    };
    xmodem_init(&xm, &g_mock_io, &g_cbs, &cfg);
    xmodem_start_recv(&xm);

    sim_peer_t sim;
    sim_init(&sim, SIM_ROLE_SENDER, SIM_PROTO_CHECKSUM, g_test_data, TEST_DATA_SIZE);

    run_protocol_loop(&xm, &sim);

    TEST_ASSERT_TRUE(g_transfer_done);
    TEST_ASSERT_EQUAL_INT(0, g_transfer_error);
    TEST_ASSERT_EQUAL_INT(384, g_file_len);
    TEST_ASSERT_EQUAL_MEMORY(g_test_data, g_file_buf, TEST_DATA_SIZE);
    
    /* 验证已降级 */
    TEST_ASSERT_EQUAL_INT(XMODEM_MODE_STANDARD, xm.current_mode);
}

/* --- 测试组2：DUT作为发送方 --- */

/* 测试用例5：DUT作为发送方（CRC） */
TEST_CASE(xmodem_tx_crc) {
    reset_test_env();
    setup_test_data();

    xmodem_t xm;
    memset(&xm, 0, sizeof(xm)); // Important!
    xmodem_config_t cfg = {
        .is_transmitter = true,
        .mode = XMODEM_MODE_CRC,
        .recv_buffer = g_xm_internal_buf,
        .recv_buffer_size = sizeof(g_xm_internal_buf),
        .user_data = NULL,
        .max_retries = 10
    };
    
    xmodem_init(&xm, &g_mock_io, &g_cbs, &cfg);
    xmodem_start_send(&xm, "test.bin", TEST_DATA_SIZE);
    
    /* 模拟器作为接收方 */
    sim_peer_t sim;
    sim_init(&sim, SIM_ROLE_RECEIVER, SIM_PROTO_CRC, NULL, 0);

    run_protocol_loop(&xm, &sim);
    
    TEST_ASSERT_TRUE(g_transfer_done);
    TEST_ASSERT_EQUAL_INT(0, g_transfer_error);
    TEST_ASSERT_EQUAL_MEMORY(g_test_data, sim.rx_buffer, TEST_DATA_SIZE);
    /* 验证模式 */
    TEST_ASSERT_EQUAL_INT(XMODEM_MODE_CRC, xm.current_mode);
}

/* 测试用例6：DUT作为发送方（Checksum） */
TEST_CASE(xmodem_tx_checksum) {
    reset_test_env();
    setup_test_data();

    xmodem_t xm;
    memset(&xm, 0, sizeof(xm));
    xmodem_config_t cfg = {
        .is_transmitter = true,
        .mode = XMODEM_MODE_STANDARD,
        .recv_buffer = g_xm_internal_buf,
        .recv_buffer_size = sizeof(g_xm_internal_buf),
        .user_data = NULL,
        .max_retries = 10
    };
    
    xmodem_init(&xm, &g_mock_io, &g_cbs, &cfg);
    xmodem_start_send(&xm, "test_sum.bin", TEST_DATA_SIZE);
    
    sim_peer_t sim;
    sim_init(&sim, SIM_ROLE_RECEIVER, SIM_PROTO_CHECKSUM, NULL, 0);

    run_protocol_loop(&xm, &sim);
    
    TEST_ASSERT_TRUE(g_transfer_done);
    TEST_ASSERT_EQUAL_INT(0, g_transfer_error);
    TEST_ASSERT_EQUAL_MEMORY(g_test_data, sim.rx_buffer, TEST_DATA_SIZE);
    
    TEST_ASSERT_EQUAL_INT(XMODEM_MODE_STANDARD, xm.current_mode);
}

/* 测试用例7：DUT作为发送方（1K CRC） */
TEST_CASE(xmodem_tx_1k) {
    reset_test_env();
    setup_test_data();

    xmodem_t xm;
    memset(&xm, 0, sizeof(xm));
    xmodem_config_t cfg = {
        .is_transmitter = true,
        .mode = XMODEM_MODE_1K,
        .recv_buffer = g_xm_internal_buf,
        .recv_buffer_size = sizeof(g_xm_internal_buf),
        .user_data = NULL,
        .max_retries = 10
    };
    
    xmodem_init(&xm, &g_mock_io, &g_cbs, &cfg);
    xmodem_start_send(&xm, "test_1k.bin", TEST_DATA_SIZE); /* 300字节 */

    sim_peer_t sim;
    /*
     * 如果配置为XMODEM-1K接收方，模拟器期望1K数据包。
     * 但XMODEM协议会协商：
     * XMODEM-1K发送方等待'C'，然后发送STX（1024字节）数据包。
     * 接收方发送'C'。
     */
    sim_init(&sim, SIM_ROLE_RECEIVER, SIM_PROTO_1K_CRC, NULL, 0);

    run_protocol_loop(&xm, &sim);

    TEST_ASSERT_TRUE(g_transfer_done);
    TEST_ASSERT_EQUAL_INT(0, g_transfer_error);
    TEST_ASSERT_EQUAL_INT(1024, sim.rx_len);
    TEST_ASSERT_EQUAL_MEMORY(g_test_data, sim.rx_buffer, TEST_DATA_SIZE);
    TEST_ASSERT_EQUAL_INT(X_CTRLZ, sim.rx_buffer[TEST_DATA_SIZE]);
    
    TEST_ASSERT_EQUAL_INT(XMODEM_MODE_1K, xm.current_mode);
}
#endif
