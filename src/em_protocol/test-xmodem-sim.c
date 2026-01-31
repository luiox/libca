#if TEST_ENABLE
#include "xmodem.h"
#include "../em_test/test.h"
#include "../em_util/crc.h"
#include <string.h>
#include <stdio.h>

/* =========================================================================
 * 1. Mock Transport & Common Utilities
 * ========================================================================= */

// XMODEM Constants
#define X_SOH 0x01
#define X_STX 0x02
#define X_EOT 0x04
#define X_ACK 0x06
#define X_NAK 0x15
#define X_CAN 0x18
#define X_CRC 'C'
#define X_SUB 0x1A

// Log buffer for what the DUT (Device Under Test) sent out via io->write
static u8 g_dut_tx_buf[4096]; 
static usize g_dut_tx_len = 0;

static i32 mock_write(transport_t* self, const u8 *buf, usize len) {
    if (g_dut_tx_len + len <= sizeof(g_dut_tx_buf)) {
        memcpy(g_dut_tx_buf + g_dut_tx_len, buf, len);
        g_dut_tx_len += len;
    }
    return len;
}

static i32 mock_read(transport_t* self, u8 *buf, usize len, u32 timeout_ms) {
    return 0; // Not used in push model
}

static transport_t g_mock_io = {
    .write = mock_write,
    .read = mock_read,
    .flush = NULL
};

// Clear the global mock buffer
static void mock_io_clear() {
    g_dut_tx_len = 0;
}

// File Interface Mocks
static u8 g_file_buf[1024 * 10]; // Max 10KB file for test
static usize g_file_len = 0;
static bool g_transfer_done = false;
static i32 g_transfer_error = 0;

static i32 on_recv(void *user_data, u32 offset, const u8* data, usize len) {
    if (g_file_len + len <= sizeof(g_file_buf)) {
        memcpy(g_file_buf + g_file_len, data, len);
        g_file_len += len;
        // Verify offset matches?
        // if (offset != g_file_len - len) ... 
        return 0;
    }
    return -1;
}

static i32 on_send(void *user_data, u32 offset, u8* buf, usize len) {
    // Not used for RX tests
    return 0;
}

static void on_start(void *user_data, u32 total_size, const char* filename) {
    // Not used
}

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

static void reset_test_env() {
    g_dut_tx_len = 0;
    g_file_len = 0;
    g_transfer_done = false;
    g_transfer_error = 0;
    memset(g_file_buf, 0, sizeof(g_file_buf));
}

// Internal buffer for xmodem instance (required by xmodem implementation)
static u8 g_xm_internal_buf[2048];

/* =========================================================================
 * 2. Peer Simulator (The "Other" Side)
 * ========================================================================= */

typedef enum {
    SIM_ROLE_SENDER,   // Simulator acts as Sender, DUT is Receiver
    SIM_ROLE_RECEIVER  // Simulator acts as Receiver, DUT is Sender
} sim_role_t;

typedef enum {
    SIM_PROTO_CHECKSUM,
    SIM_PROTO_CRC,
    SIM_PROTO_1K_CRC
} sim_proto_t;

typedef struct {
    sim_role_t role;
    sim_proto_t proto;
    
    // Data handling
    const u8* tx_data;     // Data sim wants to send
    usize tx_total_len;
    
    u8 rx_buffer[1024*10]; // Data sim received
    usize rx_len;

    // State
    usize offset;
    u8 seq;                // Expected seq (Rx) or Next seq (Tx)
    bool finished;
    
    bool sim_started;      // Has the simulation handshake started?
    bool handshake_done;
} sim_peer_t;

static void sim_init(sim_peer_t* sim, sim_role_t role, sim_proto_t proto, const u8* data, usize len) {
    memset(sim, 0, sizeof(sim_peer_t));
    sim->role = role;
    sim->proto = proto;
    sim->tx_data = data;
    sim->tx_total_len = len;
    sim->seq = 1;
}

// Generate a packet into buffer, return length
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

// Simulates the logic of the 'other' device
// in_buf: what DUT sent to Sim
// out_buf: what Sim wants to send to DUT (returns len)
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
        if (in_len >= 132) {
             u8 header = in_buf[0];
             if (header == X_SOH || header == X_STX) {
                 u8 seq = in_buf[1];
                 u8 seq_inv = in_buf[2];
                 if ((u8)(seq + seq_inv) != 0xFF) {
                     out_buf[out_len++] = X_NAK;
                 } else {
                     // Good packet (Simulating perfect transport)
                     usize data_len = (header == X_STX) ? 1024 : 128;
                     memcpy(sim->rx_buffer + sim->rx_len, &in_buf[3], data_len);
                     sim->rx_len += data_len;
                     out_buf[out_len++] = X_ACK;
                 }
             }
        }
        else if (has_EOT) {
            out_buf[out_len++] = X_ACK;
            sim->finished = true;
        }
    }

    return out_len;
}

/* =========================================================================
 * 3. Test Runner Loop
 * ========================================================================= */
static void run_protocol_loop(xmodem_t* dut, sim_peer_t* sim) {
    int max_ticks = 2000; // 200s equivalent time
    u8 sim_out_buf[2048];
    
    printf("Starting loop...\n");
    while (max_ticks-- > 0 && !sim->finished && !g_transfer_done) {
        // 1. Tick DUT
        // printf("Tick %d\n", max_ticks);
        xmodem_tick(dut, 100);

        // 2. Read what DUT sent
        // g_dut_tx_buf contains [0...g_dut_tx_len]
        
        // 3. Step Sim
        usize sim_out_len = sim_step(sim, g_dut_tx_buf, g_dut_tx_len, sim_out_buf);
        
        // Clear DUT captured buf
        mock_io_clear();

        // 4. Feed Sim output to DUT
        if (sim_out_len > 0) {
            // printf("Sim sent %zu bytes\n", sim_out_len);
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
 * 4. Test Cases
 * ========================================================================= */

// --- Test Group 1: DUT as Receiver ---
#define TEST_DATA_SIZE 300
static u8 g_test_data[TEST_DATA_SIZE];

static void setup_test_data() {
    for(int i=0; i<TEST_DATA_SIZE; i++) g_test_data[i] = (u8)i;
}

// Case 1: Standard XMODEM (Checksum)
TEST_CASE(xmodem_rx_std_checksum) {
    reset_test_env();
    setup_test_data();

    // DUT Setup
    xmodem_t xm;
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

    // Sim Setup: Sender, Checksum
    sim_peer_t sim;
    sim_init(&sim, SIM_ROLE_SENDER, SIM_PROTO_CHECKSUM, g_test_data, TEST_DATA_SIZE);

    run_protocol_loop(&xm, &sim);

    TEST_ASSERT_TRUE(g_transfer_done);
    TEST_ASSERT_EQUAL_INT(0, g_transfer_error);
    // 3 packets * 128 = 384
    TEST_ASSERT_EQUAL_INT(384, g_file_len);
    TEST_ASSERT_EQUAL_MEMORY(g_test_data, g_file_buf, TEST_DATA_SIZE);
}

// Case 2: XMODEM CRC
TEST_CASE(xmodem_rx_crc) {
    reset_test_env();
    setup_test_data();

    xmodem_t xm;
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
    // XMODEM sends in multiples of 128 bytes. 300 bytes -> 3 packets -> 384 bytes.
    TEST_ASSERT_EQUAL_INT(384, g_file_len); 
    TEST_ASSERT_EQUAL_MEMORY(g_test_data, g_file_buf, TEST_DATA_SIZE);
}

// Case 3: XMODEM 1K
TEST_CASE(xmodem_rx_1k) {
    reset_test_env();
    // Increase data to ensure we use 1K packets (need > 128 bytes, ideally > 1024 to see transitions but Sim uses 1K if Proto is 1K)
    setup_test_data(); // 300 bytes

    xmodem_t xm;
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
    // 1K Xmodem pads to 1024 bytes
    TEST_ASSERT_EQUAL_INT(1024, g_file_len);
    TEST_ASSERT_EQUAL_MEMORY(g_test_data, g_file_buf, TEST_DATA_SIZE);
}

// Case 4: XMODEM Fallback (CRC -> Checksum)
TEST_CASE(xmodem_rx_fallback) {
    reset_test_env();
    setup_test_data();

    xmodem_t xm;
    xmodem_config_t cfg = {
        .is_transmitter = false,
        .mode = XMODEM_MODE_CRC, // Default starts with CRC
        .recv_buffer = g_xm_internal_buf,
        .recv_buffer_size = sizeof(g_xm_internal_buf),
        .user_data = NULL,
        .max_retries = 10
    };
    xmodem_init(&xm, &g_mock_io, &g_cbs, &cfg);
    xmodem_start_recv(&xm);

    // Sim is Checksum ONLY. It will ignore 'C' sent by DUT initially.
    sim_peer_t sim;
    sim_init(&sim, SIM_ROLE_SENDER, SIM_PROTO_CHECKSUM, g_test_data, TEST_DATA_SIZE);

    run_protocol_loop(&xm, &sim);

    TEST_ASSERT_TRUE(g_transfer_done);
    TEST_ASSERT_EQUAL_INT(0, g_transfer_error);
    // 384 bytes = 3 packets of 128
    TEST_ASSERT_EQUAL_INT(384, g_file_len);
    TEST_ASSERT_EQUAL_MEMORY(g_test_data, g_file_buf, TEST_DATA_SIZE);
    
    // Verify that we fell back
    TEST_ASSERT_EQUAL_INT(XMODEM_MODE_STANDARD, xm.current_mode);
}

// --- Test Group 2: DUT as Sender ---

// Case 4: DUT Send CRC
// TEST_CASE(xmodem_tx_crc) {
//     reset_test_env();
//     setup_test_data();
//
//     xmodem_t xm;
//     xmodem_config_t cfg = {
//         .is_transmitter = true,
//         .mode = XMODEM_MODE_CRC,
//         .recv_buffer = g_xm_internal_buf,
//         .recv_buffer_size = sizeof(g_xm_internal_buf),
//         .user_data = NULL,
//         .max_retries = 10
//     };
//     // Note: Actual TX requires file reading callback or similar which might not be fully hooked up in this mock unless we mock file system read
//     // For now we test if it attempts to handshake
//    
//     xmodem_init(&xm, &g_mock_io, &g_cbs, &cfg);
//     xmodem_start_send(&xm, "test.bin", TEST_DATA_SIZE);
//    
//     sim_peer_t sim;
//     sim_init(&sim, SIM_ROLE_RECEIVER, SIM_PROTO_CRC, NULL, 0);
//
//     run_protocol_loop(&xm, &sim);
//    
//     // We expect it to finish (or timeout if data not provided, but here we just check loop runs)
//     // Real implementation of TX needs 'cbs->read' or similar to get data?
//     // Let's assume xmodem_start_send initiates it.
// }

#endif
