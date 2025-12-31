#include "em_log.h"
#include "em_log_internal.h"
#include "ringbuffer.h"
#include "async.h"
#include "soft_timer.h"
#include "cpu_port.h"
#include <stdio.h>
#include <string.h>

#ifndef EM_LOG_RB_SIZE
#define EM_LOG_RB_SIZE 2048
#endif

#ifndef EM_LOG_FORMAT_BUF_SIZE
#define EM_LOG_FORMAT_BUF_SIZE 128
#endif

#ifndef EM_LOG_MAX_TAG_FILTERS
#define EM_LOG_MAX_TAG_FILTERS 8
#endif

typedef struct {
    const char* tag;
    em_log_level_t level;
} log_tag_filter_t;

static uint8_t g_log_rb_mem[EM_LOG_RB_SIZE];
static ringbuffer_t g_log_rb;
static em_log_backend_t* g_backend_list = NULL;
static em_log_level_t g_log_level = EM_LOG_LEVEL_DEFAULT;
static log_tag_filter_t g_tag_filters[EM_LOG_MAX_TAG_FILTERS];
static int g_tag_filter_count = 0;
static async_t* g_async = NULL;
static volatile bool g_log_task_active = false;
static volatile uint32_t g_log_drop_count = 0;

static void log_process_task(void* arg);

void em_log_init(void) {
    ringbuffer_init(&g_log_rb, g_log_rb_mem, EM_LOG_RB_SIZE);
    g_backend_list = NULL;
    g_log_task_active = false;
    g_log_drop_count = 0;
    g_tag_filter_count = 0;
}

void em_log_set_async(void* async) {
    g_async = (async_t*)async;
}

void em_log_backend_register(em_log_backend_t* backend) {
    if (!backend) return;
    
    if (backend->init) {
        backend->init(backend);
    }
    
    EM_CPU_ENTER_CRITICAL();
    backend->next = g_backend_list;
    g_backend_list = backend;
    EM_CPU_EXIT_CRITICAL();
}

void em_log_set_level(em_log_level_t level) {
    g_log_level = level;
}

void em_log_set_tag_level(const char* tag, em_log_level_t level) {
    if (!tag) return;
    
    EM_CPU_ENTER_CRITICAL();
    // Check if exists, update
    for (int i = 0; i < g_tag_filter_count; i++) {
        if (strcmp(g_tag_filters[i].tag, tag) == 0) {
            g_tag_filters[i].level = level;
            EM_CPU_EXIT_CRITICAL();
            return;
        }
    }
    // Add new
    if (g_tag_filter_count < EM_LOG_MAX_TAG_FILTERS) {
        g_tag_filters[g_tag_filter_count].tag = tag;
        g_tag_filters[g_tag_filter_count].level = level;
        g_tag_filter_count++;
    }
    EM_CPU_EXIT_CRITICAL();
}

void em_log_write(em_log_level_t level, const char* tag, const char* fmt, ...) {
    em_log_level_t filter_level = g_log_level;
    
    // Check tag specific level
    if (g_tag_filter_count > 0 && tag) {
        for (int i = 0; i < g_tag_filter_count; i++) {
            // Try pointer comparison first for speed, then string comparison
            if (g_tag_filters[i].tag == tag || strcmp(g_tag_filters[i].tag, tag) == 0) {
                filter_level = g_tag_filters[i].level;
                break;
            }
        }
    }

    if (level > filter_level) return;

    // Prepare Header
    log_packet_header_t header;
    uint32_t now_ms = time_get_ms();
    // time_get_us() returns microsecond offset within the current millisecond (0-999)
    uint16_t now_us_offset = (uint16_t)(time_get_us() % 1000); 

    header.time_sec = now_ms / 1000;
    header.time_ms  = (uint16_t)(now_ms % 1000);
    header.time_us  = now_us_offset;
    
    header.level = level;
    header.tag = tag;
    header.reserved = 0;

    // Format Payload
    char buf[EM_LOG_FORMAT_BUF_SIZE];
    va_list args;
    va_start(args, fmt);
    int len = vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);

    if (len < 0) len = 0;
    if (len >= sizeof(buf)) len = sizeof(buf) - 1;
    
    header.total_len = sizeof(header) + len + 1; // +1 for null terminator

    EM_CPU_ENTER_CRITICAL();
    if (ringbuffer_free(&g_log_rb) >= header.total_len) {
        ringbuffer_write(&g_log_rb, (uint8_t*)&header, sizeof(header));
        ringbuffer_write(&g_log_rb, (uint8_t*)buf, len + 1);
        
        // Edge Trigger Async Task
        if (g_async && !g_log_task_active) {
            if (async_submit(g_async, log_process_task, NULL)) {
                g_log_task_active = true;
            }
        }
    } else {
        g_log_drop_count++;
    }
    EM_CPU_EXIT_CRITICAL();
}

static void log_process_task(void* arg) {
    (void)arg;
    log_packet_header_t header;
    char payload_buf[EM_LOG_FORMAT_BUF_SIZE + 1];

    while (1) {
        // Peek header to check if we have enough data
        if (ringbuffer_used(&g_log_rb) < sizeof(header)) {
            break;
        }
        
        ringbuffer_peek(&g_log_rb, (uint8_t*)&header, sizeof(header));

        if (ringbuffer_used(&g_log_rb) < header.total_len) {
            // Should not happen if write is atomic
            break;
        }

        // Read header
        ringbuffer_read(&g_log_rb, (uint8_t*)&header, sizeof(header));
        
        // Read payload
        uint16_t payload_len = header.total_len - sizeof(header);
        // Safety check
        if (payload_len > sizeof(payload_buf)) {
            ringbuffer_skip(&g_log_rb, payload_len);
            continue;
        }
        
        ringbuffer_read(&g_log_rb, (uint8_t*)payload_buf, payload_len);
        
        // Construct record
        em_log_record_t record;
        record.level = (em_log_level_t)header.level;
        record.time_sec = header.time_sec;
        record.time_ms  = header.time_ms;
        record.time_us  = header.time_us;
        record.tag = header.tag;
        record.payload = payload_buf;
        record.payload_len = payload_len > 0 ? payload_len - 1 : 0; 
        
        // Dispatch
        em_log_backend_t* backend = g_backend_list;
        while (backend) {
            if (backend->enabled && record.level <= backend->min_level) {
                if (backend->output) {
                    backend->output(backend, &record);
                }
            }
            backend = backend->next;
        }
    }

    EM_CPU_ENTER_CRITICAL();
    g_log_task_active = false;
    // Double check
    if (ringbuffer_used(&g_log_rb) > 0) {
        if (g_async && async_submit(g_async, log_process_task, NULL)) {
            g_log_task_active = true;
        }
    }
    EM_CPU_EXIT_CRITICAL();
}

void em_log_flush(void) {
    // Warning: Not thread safe if async task is preemptive.
    // Intended for use when async task is not running or in single threaded mode.
    log_process_task(NULL);
}

void em_log_panic(void) {
    log_packet_header_t header;
    char payload_buf[EM_LOG_FORMAT_BUF_SIZE + 1];

    while (ringbuffer_used(&g_log_rb) >= sizeof(header)) {
        ringbuffer_peek(&g_log_rb, (uint8_t*)&header, sizeof(header));
        if (ringbuffer_used(&g_log_rb) < header.total_len) break;
        
        ringbuffer_read(&g_log_rb, (uint8_t*)&header, sizeof(header));
        uint16_t payload_len = header.total_len - sizeof(header);
        if (payload_len > sizeof(payload_buf)) {
             ringbuffer_skip(&g_log_rb, payload_len);
             continue;
        }
        ringbuffer_read(&g_log_rb, (uint8_t*)payload_buf, payload_len);
        
        em_log_record_t record;
        record.level = (em_log_level_t)header.level;
        record.time_sec = header.time_sec;
        record.time_ms  = header.time_ms;
        record.time_us  = header.time_us;
        record.tag = header.tag;
        record.payload = payload_buf;
        record.payload_len = payload_len;

        em_log_backend_t* backend = g_backend_list;
        while (backend) {
            if (backend->enabled && backend->panic_output) {
                backend->panic_output(backend, &record);
            }
            backend = backend->next;
        }
    }
}

#if TEST_ENABLE
#include "../em_test/test.h"
#include <stdio.h>

// --- Backend 1: Binary File (Raw Record) ---
static FILE* g_bin_file = NULL;
static void bin_backend_output(em_log_backend_t* backend, const em_log_record_t* record) {
    (void)backend;
    if (!g_bin_file) return;
    
    // Simple serialization: [Sec(4)][Ms(2)][Us(2)][Level(1)][TagLen(1)][Tag(N)][PayloadLen(2)][Payload(M)]
    fwrite(&record->time_sec, sizeof(record->time_sec), 1, g_bin_file);
    fwrite(&record->time_ms, sizeof(record->time_ms), 1, g_bin_file);
    fwrite(&record->time_us, sizeof(record->time_us), 1, g_bin_file);
    
    uint8_t lvl = (uint8_t)record->level;
    fwrite(&lvl, sizeof(lvl), 1, g_bin_file);
    
    uint8_t tag_len = (uint8_t)strlen(record->tag);
    fwrite(&tag_len, sizeof(tag_len), 1, g_bin_file);
    fwrite(record->tag, 1, tag_len, g_bin_file);
    
    uint16_t pl_len = (uint16_t)record->payload_len;
    fwrite(&pl_len, sizeof(pl_len), 1, g_bin_file);
    fwrite(record->payload, 1, pl_len, g_bin_file);
}

static em_log_backend_t g_bin_backend = {
    .name = "BIN_FILE",
    .min_level = EM_LOG_INFO,
    .enabled = true,
    .output = bin_backend_output,
};

// --- Backend 2: Text File (Formatted) ---
static FILE* g_txt_file = NULL;
static void txt_backend_output(em_log_backend_t* backend, const em_log_record_t* record) {
    (void)backend;
    if (!g_txt_file) return;
    
    fprintf(g_txt_file, "[%u.%03u.%03u][%d][%s] %s\n", 
            record->time_sec, record->time_ms, record->time_us,
            record->level, record->tag, record->payload);
}

static em_log_backend_t g_txt_backend = {
    .name = "TXT_FILE",
    .min_level = EM_LOG_INFO,
    .enabled = true,
    .output = txt_backend_output,
};

// --- Backend 3: Console (Formatted) ---
static void console_backend_output(em_log_backend_t* backend, const em_log_record_t* record) {
    (void)backend;
    // Using printf for console output
    printf("[CONSOLE] [%u.%03u.%03u] Level:%d Tag:%s Msg:%s\n", 
           record->time_sec, record->time_ms, record->time_us,
           record->level, record->tag, record->payload);
}

static em_log_backend_t g_console_backend = {
    .name = "CONSOLE",
    .min_level = EM_LOG_INFO,
    .enabled = true,
    .output = console_backend_output,
};

TEST_CASE(log_multi_backend_demo) {
    em_log_init();
    
    // Open files
    g_bin_file = fopen("test_log.bin", "wb");
    g_txt_file = fopen("test_log.txt", "w");
    
    TEST_ASSERT_NOT_NULL(g_bin_file);
    TEST_ASSERT_NOT_NULL(g_txt_file);

    // Register backends
    em_log_backend_register(&g_bin_backend);
    em_log_backend_register(&g_txt_backend);
    em_log_backend_register(&g_console_backend);
    
    printf("\n--- Starting Multi-Backend Log Test ---\n");

    // Write logs
    EM_LOG_I("MAIN", "System starting...");
    EM_LOG_W("SENSOR", "Temperature high: %d", 85);
    EM_LOG_E("MOTOR", "Overcurrent detected!");
    EM_LOG_D("DEBUG", "This should be ignored by default min_level=INFO");
    
    // Flush to process (Synchronous simulation)
    em_log_flush();
    
    printf("--- Logs Flushed ---\n");

    // Close files
    fclose(g_bin_file);
    fclose(g_txt_file);
    g_bin_file = NULL;
    g_txt_file = NULL;
    
    // Verify files exist
    FILE* f = fopen("test_log.bin", "rb");
    TEST_ASSERT_NOT_NULL(f);
    if(f) {
        // Check file size > 0
        fseek(f, 0, SEEK_END);
        long size = ftell(f);
        TEST_ASSERT_TRUE(size > 0);
        fclose(f);
        printf("Binary log file size: %ld bytes\n", size);
    }
    
    f = fopen("test_log.txt", "r");
    TEST_ASSERT_NOT_NULL(f);
    if(f) {
        char line[128];
        if(fgets(line, sizeof(line), f)) {
            printf("First line of TXT log: %s", line);
        }
        fclose(f);
    }
}

// --- Async & Threading Test ---
#include <windows.h>
#include <process.h>

// Mock Time Provider
static volatile uint32_t g_mock_ms = 0;
static volatile uint16_t g_mock_us = 0;
static bool g_time_thread_running = true;

static uint32_t mock_get_time_ms(void) {
    return g_mock_ms;
}

static uint32_t mock_get_time_us(void) {
    return g_mock_us;
}

static unsigned __stdcall time_mock_thread(void* arg) {
    (void)arg;
    while (g_time_thread_running) {
        Sleep(1); // 1ms tick
        g_mock_ms++;
        // Simulate microsecond offset changing
        g_mock_us = (g_mock_us + 123) % 1000;
    }
    return 0;
}

static volatile bool g_thread_running = true;
static async_t g_test_async;
static uint8_t g_async_mem[1024];

// Simulated ISR thread (High priority producer)
static unsigned __stdcall isr_thread_func(void* arg) {
    (void)arg;
    int count = 0;
    while (g_thread_running) {
        // Simulate ISR logging
        EM_LOG_I("ISR", "Interrupt event %d", count++);
        Sleep(10); // 10ms interval
    }
    return 0;
}

// Simulated Main Loop (Consumer)
static void main_loop_process(void) {
    // Process async tasks (which includes log dispatching)
    async_process(&g_test_async, 0); // 0 = process all
}

TEST_CASE(log_async_multithread) {
    em_log_init();
    
    // Setup Mock Time Provider
    g_mock_ms = 0;
    g_mock_us = 0;
    g_time_thread_running = true;
    time_set_ms_provider(mock_get_time_ms);
    time_set_us_provider(mock_get_time_us);
    HANDLE hTimeThread = (HANDLE)_beginthreadex(NULL, 0, time_mock_thread, NULL, 0, NULL);
    
    // Initialize Async
    async_init(&g_test_async, g_async_mem, sizeof(g_async_mem), NULL);
    em_log_set_async(&g_test_async);
    
    // Register Console Backend
    em_log_backend_register(&g_console_backend);
    
    printf("\n--- Starting Async Multithread Test ---\n");
    
    // Start ISR thread
    g_thread_running = true;
    HANDLE hThread = (HANDLE)_beginthreadex(NULL, 0, isr_thread_func, NULL, 0, NULL);
    TEST_ASSERT_TRUE(hThread != NULL);
    
    // Main loop simulation for 500ms
    uint32_t start_time = GetTickCount();
    while (GetTickCount() - start_time < 500) {
        // Simulate main loop work
        EM_LOG_D("MAIN", "Main loop working...");
        
        // Process logs
        main_loop_process();
        
        Sleep(5); // Yield CPU
    }
    
    // Stop thread
    g_thread_running = false;
    WaitForSingleObject(hThread, INFINITE);
    CloseHandle(hThread);
    
    // Stop Time Thread
    g_time_thread_running = false;
    WaitForSingleObject(hTimeThread, INFINITE);
    CloseHandle(hTimeThread);
    
    // Flush remaining logs
    main_loop_process();
    
    printf("--- Async Test Finished ---\n");
}

// --- Advanced Features Test ---

// --- Helper for Verification ---
static char g_test_output_buf[256];
static em_log_record_t g_last_record;

static void test_backend_output(em_log_backend_t* backend, const em_log_record_t* record) {
    (void)backend;
    g_last_record = *record;
    // Copy payload to buffer for verification
    size_t len = record->payload_len < sizeof(g_test_output_buf) ? record->payload_len : sizeof(g_test_output_buf) - 1;
    memcpy(g_test_output_buf, record->payload, len);
    g_test_output_buf[len] = '\0';
}

static em_log_backend_t g_test_backend = {
    .name = "TEST",
    .min_level = EM_LOG_INFO,
    .enabled = true,
    .output = test_backend_output,
    .panic_output = NULL,
    .flush = NULL
};

// Custom macro to include file and line number
#define EM_LOG_TRACE(fmt, ...) EM_LOG_V("TRACE", "[%s:%d] " fmt, __FILE__, __LINE__, ##__VA_ARGS__)

TEST_CASE(log_tags_and_levels) {
    em_log_init();
    
    // Setup backend to capture output
    // We reuse g_test_backend which captures the last record
    // Backend allows everything down to VERBOSE so we can test the frontend filtering
    g_test_backend.min_level = EM_LOG_VERBOSE; 
    em_log_backend_register(&g_test_backend);
    
    // Set Global Level to INFO
    em_log_set_level(EM_LOG_INFO);

    // 1. Test Default Global Filter (INFO)
    // DEBUG should be filtered out
    memset(g_test_output_buf, 0, sizeof(g_test_output_buf));
    EM_LOG_D("BLE", "BLE Debug");
    em_log_flush();
    TEST_ASSERT_EQUAL_STRING("", g_test_output_buf);

    // INFO should pass
    EM_LOG_I("BLE", "BLE Info");
    em_log_flush();
    TEST_ASSERT_EQUAL_STRING("BLE Info", g_test_output_buf);

    // 2. Test Tag Specific Filter
    // Set "WIFI" to DEBUG (Allowing more verbose logs for WIFI)
    em_log_set_tag_level("WIFI", EM_LOG_DEBUG);

    // WIFI DEBUG should now pass
    memset(g_test_output_buf, 0, sizeof(g_test_output_buf));
    EM_LOG_D("WIFI", "WIFI Debug Packet");
    em_log_flush();
    TEST_ASSERT_EQUAL_STRING("WIFI Debug Packet", g_test_output_buf);
    TEST_ASSERT_EQUAL_STRING("WIFI", g_last_record.tag);

    // BLE DEBUG should still be filtered (Global is INFO)
    memset(g_test_output_buf, 0, sizeof(g_test_output_buf));
    EM_LOG_D("BLE", "BLE Debug 2");
    em_log_flush();
    TEST_ASSERT_EQUAL_STRING("", g_test_output_buf);

    // 3. Test Tag Specific Filter (Restricting)
    // Set "NOISY" to ERROR (Only allow errors)
    em_log_set_tag_level("NOISY", EM_LOG_ERROR);

    // NOISY INFO should be filtered (Global is INFO, but Tag is ERROR)
    memset(g_test_output_buf, 0, sizeof(g_test_output_buf));
    EM_LOG_I("NOISY", "Noisy Info");
    em_log_flush();
    TEST_ASSERT_EQUAL_STRING("", g_test_output_buf);

    // NOISY ERROR should pass
    EM_LOG_E("NOISY", "Noisy Error");
    em_log_flush();
    TEST_ASSERT_EQUAL_STRING("Noisy Error", g_test_output_buf);
    TEST_ASSERT_EQUAL_STRING("NOISY", g_last_record.tag);
}

TEST_CASE(log_trace_macro) {
    em_log_init();
    // Enable VERBOSE globally and for backend
    em_log_set_level(EM_LOG_VERBOSE);
    g_test_backend.min_level = EM_LOG_VERBOSE;
    em_log_backend_register(&g_test_backend);
    
    memset(g_test_output_buf, 0, sizeof(g_test_output_buf));
    
    int line = __LINE__ + 1;
    EM_LOG_TRACE("Variable x=%d", 42);
    em_log_flush();
    
    // Expected output: "[filename:line] Variable x=42"
    char expected[256];
    snprintf(expected, sizeof(expected), "[%s:%d] Variable x=%d", __FILE__, line, 42);
    
    TEST_ASSERT_EQUAL_STRING(expected, g_test_output_buf);
    TEST_ASSERT_EQUAL_STRING("TRACE", g_last_record.tag);
}

TEST_CASE(log_tags_console_demo) {
    em_log_init();
    em_log_backend_register(&g_console_backend);
    
    printf("\n--- Multi-Tag Console Demo ---\n");
    EM_LOG_I("WIFI", "Connecting to AP...");
    EM_LOG_I("TCP", "Handshake success");
    EM_LOG_W("HEAP", "Memory low: %d bytes free", 1024);
    EM_LOG_E("FLASH", "Write error at 0x08000000");
    
    // Custom Trace with File/Line
    #define LOG_WITH_LOC(level, tag, fmt, ...) \
        em_log_write(level, tag, "[%s:%d] " fmt, __FILE__, __LINE__, ##__VA_ARGS__)
        
    LOG_WITH_LOC(EM_LOG_ERROR, "ASSERT", "Pointer is NULL");
    
    em_log_flush();
    printf("--- End Demo ---\n");
}
#endif

