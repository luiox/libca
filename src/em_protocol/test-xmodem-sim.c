#if TEST_ENABLE
#include "xmodem.h"
#include "../em_test/test.h"
#include "em_util/crc.h"
#include <string.h>

/* --- Mock Transport & Environment --- */

static u8 g_tx_log_buf[4096]; // Log what the receiver writes
static usize g_tx_log_len = 0;

static i32 mock_write(transport_t* self, const u8 *buf, usize len) {
    if (g_tx_log_len + len <= sizeof(g_tx_log_buf)) {
        memcpy(g_tx_log_buf + g_tx_log_len, buf, len);
        g_tx_log_len += len;
    }
    return len;
}

static i32 mock_read(transport_t* self, u8 *buf, usize len, u32 timeout_ms) {
    return 0; // Not used
}

static transport_t g_mock_io = {
    .write = mock_write,
    .read = mock_read,
    .flush = NULL
};

/* --- Callbacks to verify received data --- */

static u8 g_recv_file_buf[4096];
static usize g_recv_total_len = 0;
static bool g_transfer_done = false;
static i32 g_transfer_error = 0;

static i32 on_file_data(const u8* data, usize len, void* user_data) {
    if (g_recv_total_len + len <= sizeof(g_recv_file_buf)) {
        memcpy(g_recv_file_buf + g_recv_total_len, data, len);
        g_recv_total_len += len;
        return 0;
    }
    return -1;
}

static void on_file_done(void* user_data) {
    g_transfer_done = true;
}

static void on_file_error(i32 error_code, void* user_data) {
    g_transfer_error = error_code;
}

static file_transfer_cbs_t g_cbs = {
    .on_data = on_file_data,
    .on_done = on_file_done,
    .on_error = on_file_error
};

/* --- Software Sender Simulator --- */

#define SIM_SOH 0x01
#define SIM_EOT 0x04
#define SIM_ACK 0x06
#define SIM_NAK 0x15
#define SIM_CAN 0x18
#define SIM_CRC 'C'

typedef struct {
    const u8* data;
    usize total_len;
    
    // State
    usize offset;
    u8 seq;
    bool waiting_start;
    bool sent_eot;
    bool finished;
} sim_sender_t;

static void sim_sender_init(sim_sender_t* sim, const u8* data, usize len) {
    sim->data = data;
    sim->total_len = len;
    sim->offset = 0;
    sim->seq = 1;
    sim->waiting_start = true;
    sim->sent_eot = false;
    sim->finished = false;
}

// Generate packet data into buf
// Returns length generated
static usize sim_sender_generate_packet(sim_sender_t* sim, u8* out_buf) {
    usize idx = 0;
    
    // Header
    out_buf[idx++] = SIM_SOH;
    out_buf[idx++] = sim->seq;
    out_buf[idx++] = ~sim->seq;
    
    // Data (128 bytes)
    usize remaining = sim->total_len - sim->offset;
    usize chunk = remaining > 128 ? 128 : remaining;
    
    memcpy(&out_buf[idx], sim->data + sim->offset, chunk);
    // Padding
    if (chunk < 128) {
        memset(&out_buf[idx + chunk], 0x1A, 128 - chunk);
    }
    idx += 128;
    
    // CRC
    // Calculate CRC on the 128 bytes data
    u16 crc = crc16_xmodem(&out_buf[3], 128);
    out_buf[idx++] = (crc >> 8) & 0xFF;
    out_buf[idx++] = crc & 0xFF;
    
    return idx;
}

/*
 * Step the simulator based on what the receiver sent.
 * It will "push" data into the receiver via xmodem_process.
 */
static void sim_step(sim_sender_t* sim, xmodem_t* receiver) {
    // 1. Process Receiver Output (what receiver wrote to mock_io)
    // We scan g_tx_log_buf
    bool received_C = false;
    bool received_ACK = false;
    bool received_NAK = false;
    
    for (usize i = 0; i < g_tx_log_len; i++) {
        u8 ch = g_tx_log_buf[i];
        if (ch == SIM_CRC) received_C = true;
        if (ch == SIM_ACK) received_ACK = true;
        if (ch == SIM_NAK) received_NAK = true;
    }
    // Clear log after reading
    g_tx_log_len = 0;
    
    // 2. State Machine Logic
    u8 packet_buf[1024]; // Temp buffer for generated packet
    
    if (sim->finished) return;

    if (sim->waiting_start) {
        if (received_C) {
            sim->waiting_start = false;
            // Send first packet
            usize len = sim_sender_generate_packet(sim, packet_buf);
            xmodem_process(receiver, packet_buf, len);
        }
    } else if (sim->sent_eot) {
        if (received_ACK) {
            sim->finished = true;
        }
    } else {
        // Normal transmission
        if (received_ACK) {
            // ACK for previous packet
            sim->offset += 128;
            sim->seq++;
            
            if (sim->offset >= sim->total_len) {
                // Done sending data, send EOT
                packet_buf[0] = SIM_EOT;
                xmodem_process(receiver, packet_buf, 1);
                sim->sent_eot = true;
            } else {
                // Send next packet
                usize len = sim_sender_generate_packet(sim, packet_buf);
                xmodem_process(receiver, packet_buf, len);
            }
        } else if (received_NAK) {
            // Resend current packet
            usize len = sim_sender_generate_packet(sim, packet_buf);
            xmodem_process(receiver, packet_buf, len);
        } else if (received_C && sim->offset == 0) {
             // Maybe receiver timed out and sent C again before we sent first packet
             usize len = sim_sender_generate_packet(sim, packet_buf);
             xmodem_process(receiver, packet_buf, len);
        }
    }
}

TEST_CASE(xmodem_sim_receiver_basic) {
    // 1. Prepare Test Data
    u8 test_data[300]; 
    for(int i=0; i<300; i++) test_data[i] = (u8)(i & 0xFF);
    
    // 2. Setup XMODEM Receiver
    xmodem_t xm;
    xmodem_config_t cfg = {
        .is_transmitter = false,
        .mode = XMODEM_MODE_CRC,
        .user_data = NULL
        // recv_buffer is not really used in callback mode usually, dependent on impl
    };
    
    // Reset Globals
    g_tx_log_len = 0;
    g_recv_total_len = 0;
    g_transfer_done = false;
    g_transfer_error = 0;
    
    xmodem_init(&xm, &g_mock_io, &g_cbs, &cfg);
    xmodem_start_recv(&xm);
    
    // 3. Setup Simulator
    sim_sender_t sim;
    sim_sender_init(&sim, test_data, sizeof(test_data));
    
    // 4. Loop
    int max_loops = 1000;
    while (!sim.finished && max_loops-- > 0) {
        // Run Receiver Tick (advance time)
        xmodem_tick(&xm, 100); 
        
        // Run Simulator (react to receiver output, feed receiver input)
        sim_step(&sim, &xm);

        if (g_transfer_done) break;
    }
    
    // 5. Assertions
    TEST_ASSERT_TRUE(g_transfer_done);
    TEST_ASSERT_EQUAL_INT(300, g_recv_total_len);
    TEST_ASSERT_EQUAL_MEMORY(test_data, g_recv_file_buf, 300);
}

#endif
