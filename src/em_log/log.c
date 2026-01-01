#include "log.h"
#include "ringbuffer.h"
#include "async.h"
#include "soft_timer.h"
#include "cpu_port.h"

#ifdef _WIN32
#include <windows.h>
static CRITICAL_SECTION g_log_cs;
static bool g_log_cs_init = false;
#undef EM_CPU_ENTER_CRITICAL
#undef EM_CPU_EXIT_CRITICAL
#define EM_CPU_ENTER_CRITICAL() do { \
    if (!g_log_cs_init) { InitializeCriticalSection(&g_log_cs); g_log_cs_init = true; } \
    EnterCriticalSection(&g_log_cs); \
} while(0)
#define EM_CPU_EXIT_CRITICAL() LeaveCriticalSection(&g_log_cs)
#endif
#include <stdio.h>
#include <string.h>

#pragma pack(push, 1)
typedef struct {
    uint16_t total_len;  /**< Total length of the packet (Header + Payload) */
    uint8_t  level;      /**< Log level */
    uint8_t  type;       /**< Packet type: 0=String, 1=ISR Args */
    uint8_t  num_args;   /**< Number of arguments (Only for ISR type) */
    uint32_t time_sec;   /**< Seconds since boot */
    uint16_t time_ms;    /**< Milliseconds part (0-999) */
    uint16_t time_us;    /**< Microseconds part (0-999) */
    const char* tag;     /**< Pointer to static tag string */
} log_packet_header_t;

#define LOG_PKT_TYPE_STRING   0
#define LOG_PKT_TYPE_ISR_ARGS 1
#pragma pack(pop)

#ifndef LOG_RB_SIZE
#define LOG_RB_SIZE 2048
#endif

#ifndef LOG_FORMAT_BUF_SIZE
#define LOG_FORMAT_BUF_SIZE 128
#endif

#ifndef LOG_MAX_TAG_FILTERS
#define LOG_MAX_TAG_FILTERS 8
#endif

typedef struct {
    const char* tag;
    log_level_t level;
} log_tag_filter_t;

static uint8_t g_log_rb_mem[LOG_RB_SIZE];
static ringbuffer_t g_log_rb;
static log_backend_t* g_backend_list = NULL;
static log_level_t g_log_level = LOG_LEVEL_DEFAULT;
static log_tag_filter_t g_tag_filters[LOG_MAX_TAG_FILTERS];
static int g_tag_filter_count = 0;
static async_t* g_async = NULL;
static volatile bool g_log_task_active = false;
static volatile uint32_t g_log_drop_count = 0;

static void log_process_task(void* arg);

void log_output_all_backends(const log_record_t* record) {
    log_backend_t* backend = g_backend_list;
    while (backend) {
        if (backend->enabled && record->level <= backend->min_level) {
            if (backend->output) {
                backend->output(backend, record);
            }
        }
        backend = backend->next;
    }
}

void log_init(void) {
    ringbuffer_init(&g_log_rb, g_log_rb_mem, LOG_RB_SIZE);
    g_backend_list = NULL;
    g_log_task_active = false;
    g_log_drop_count = 0;
    g_tag_filter_count = 0;
}

void log_set_async(void* async) {
    g_async = (async_t*)async;
}

void log_backend_register(log_backend_t* backend) {
    if (!backend) return;
    
    if (backend->init) {
        backend->init(backend);
    }
    
    EM_CPU_ENTER_CRITICAL();
    backend->next = g_backend_list;
    g_backend_list = backend;
    EM_CPU_EXIT_CRITICAL();
}

void log_set_level(log_level_t level) {
    g_log_level = level;
}

void log_set_tag_level(const char* tag, log_level_t level) {
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
    if (g_tag_filter_count < LOG_MAX_TAG_FILTERS) {
        g_tag_filters[g_tag_filter_count].tag = tag;
        g_tag_filters[g_tag_filter_count].level = level;
        g_tag_filter_count++;
    }
    EM_CPU_EXIT_CRITICAL();
}

void log_write(log_level_t level, const char* tag, const char* fmt, ...) {
    log_level_t filter_level = g_log_level;
    
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
    header.type = LOG_PKT_TYPE_STRING;

    // Format Payload
    char buf[LOG_FORMAT_BUF_SIZE];
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

void log_write_isr(log_level_t level, const char* tag, const char* fmt, int num_args, ...) {
    // 1. Global filter (Non-critical, fast)
    if (level > g_log_level) return;

    // 2. Prepare args (Stack operation, no interrupt disable)
    // Limit args to 8 to prevent stack overflow and match cache size
    if (num_args > 8) num_args = 8; 
    
    uintptr_t args_cache[8];
    va_list va;
    va_start(va, num_args);
    for (int i = 0; i < num_args; i++) {
        args_cache[i] = va_arg(va, uintptr_t);
    }
    va_end(va);

    // 3. Prepare Header
    log_packet_header_t header;
    uint32_t now_ms = time_get_ms();
    uint16_t now_us_offset = (uint16_t)(time_get_us() % 1000); 

    header.time_sec = now_ms / 1000;
    header.time_ms  = (uint16_t)(now_ms % 1000);
    header.time_us  = now_us_offset;
    header.level = level;
    header.tag = tag;
    header.type = LOG_PKT_TYPE_ISR_ARGS;
    header.num_args = (uint8_t)num_args;
    
    // Calculate total length: Header + FmtPtr + Args
    header.total_len = sizeof(header) + sizeof(const char*) + (num_args * sizeof(uintptr_t));

    // 4. Short Critical Section: Memory Copy Only
    EM_CPU_ENTER_CRITICAL();
    
    // Double check: prevent buffer full before entering critical section
    if (ringbuffer_free(&g_log_rb) >= header.total_len) {
        // Write Header
        ringbuffer_write(&g_log_rb, (uint8_t*)&header, sizeof(header));
        
        // Write Fmt Ptr
        ringbuffer_write(&g_log_rb, (uint8_t*)&fmt, sizeof(const char*));

        // Write Args (RingBuffer usually handles small writes fast)
        for (int i = 0; i < num_args; i++) {
             ringbuffer_write(&g_log_rb, (uint8_t*)&args_cache[i], sizeof(uintptr_t));
        }
    } else {
        g_log_drop_count++;
    }
    
    EM_CPU_EXIT_CRITICAL();

    // 5. Wake up consumer (Outside critical section!)
    if (g_async && !g_log_task_active) {
        // Use atomic flag or just submit. async_submit should be safe to call from ISR if implemented correctly.
        if (async_submit(g_async, log_process_task, NULL)) {
            g_log_task_active = true;
        }
    }
}

static void log_process_task(void* arg) {
    (void)arg;
    log_packet_header_t header;
    char payload_buf[LOG_FORMAT_BUF_SIZE + 1];

    while (1) {
        EM_CPU_ENTER_CRITICAL();
        // Peek header to check if we have enough data
        if (ringbuffer_used(&g_log_rb) < sizeof(header)) {
            EM_CPU_EXIT_CRITICAL();
            break;
        }
        
        ringbuffer_peek(&g_log_rb, (uint8_t*)&header, sizeof(header));

        if (ringbuffer_used(&g_log_rb) < header.total_len) {
            // Should not happen if write is atomic
            EM_CPU_EXIT_CRITICAL();
            break;
        }

        // Read header
        ringbuffer_read(&g_log_rb, (uint8_t*)&header, sizeof(header));
        
        // Read payload
        uint16_t payload_len = header.total_len - sizeof(header);
        
        if (header.type == LOG_PKT_TYPE_STRING) {
            // Safety check
            if (payload_len > sizeof(payload_buf)) {
                ringbuffer_skip(&g_log_rb, payload_len);
                EM_CPU_EXIT_CRITICAL();
                continue;
            }
            
            ringbuffer_read(&g_log_rb, (uint8_t*)payload_buf, payload_len);
            EM_CPU_EXIT_CRITICAL();
        } else if (header.type == LOG_PKT_TYPE_ISR_ARGS) {
            const char* fmt;
            uint8_t n_args = header.num_args;
            uintptr_t args[16] = {0}; // Support up to 16 args in consumer
            
            ringbuffer_read(&g_log_rb, (uint8_t*)&fmt, sizeof(const char*));
            // ringbuffer_read(&g_log_rb, &n_args, 1); // num_args is now in header
            
            for (int i = 0; i < n_args; i++) {
                uintptr_t val;
                ringbuffer_read(&g_log_rb, (uint8_t*)&val, sizeof(uintptr_t));
                if (i < 16) {
                    args[i] = val;
                }
            }
            EM_CPU_EXIT_CRITICAL();
            
            // Format now (Pass all 16 args, snprintf ignores extra ones)
            int len = snprintf(payload_buf, sizeof(payload_buf), fmt,
                args[0], args[1], args[2], args[3], args[4], args[5], args[6], args[7],
                args[8], args[9], args[10], args[11], args[12], args[13], args[14], args[15]);

            if (len < 0) len = 0;
            if (len >= sizeof(payload_buf)) len = sizeof(payload_buf) - 1;
            payload_len = len + 1; // Include null terminator for record
        } else {
            // Unknown type
            ringbuffer_skip(&g_log_rb, payload_len);
            EM_CPU_EXIT_CRITICAL();
            continue;
        }
        
        // Construct record
        log_record_t record;
        record.level = (log_level_t)header.level;
        record.time_sec = header.time_sec;
        record.time_ms  = header.time_ms;
        record.time_us  = header.time_us;
        record.tag = header.tag;
        record.payload = payload_buf;
        record.payload_len = payload_len > 0 ? payload_len - 1 : 0; 
        
        // Dispatch
        log_output_all_backends(&record);
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

void log_flush(void) {
    // Warning: Not thread safe if async task is preemptive.
    // Intended for use when async task is not running or in single threaded mode.
    log_process_task(NULL);
}

void log_panic(void) {
    log_packet_header_t header;
    char payload_buf[LOG_FORMAT_BUF_SIZE + 1];

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
        
        log_record_t record;
        record.level = (log_level_t)header.level;
        record.time_sec = header.time_sec;
        record.time_ms  = header.time_ms;
        record.time_us  = header.time_us;
        record.tag = header.tag;
        record.payload = payload_buf;
        record.payload_len = payload_len;

        log_backend_t* backend = g_backend_list;
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
static void bin_backend_output(log_backend_t* backend, const log_record_t* record) {
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

static log_backend_t g_bin_backend = {
    .name = "BIN_FILE",
    .min_level = LOG_LEVEL_INFO,
    .enabled = true,
    .output = bin_backend_output,
};

// --- Backend 2: Text File (Formatted) ---
static FILE* g_txt_file = NULL;
static void txt_backend_output(log_backend_t* backend, const log_record_t* record) {
    (void)backend;
    if (!g_txt_file) return;
    
    fprintf(g_txt_file, "[%u.%03u.%03u][%d][%s] %s\n", 
            record->time_sec, record->time_ms, record->time_us,
            record->level, record->tag, record->payload);
}

static log_backend_t g_txt_backend = {
    .name = "TXT_FILE",
    .min_level = LOG_LEVEL_INFO,
    .enabled = true,
    .output = txt_backend_output,
};

// --- Backend 3: Console (Formatted) ---
static void console_backend_output(log_backend_t* backend, const log_record_t* record) {
    (void)backend;
    // Using printf for console output
    printf("[CONSOLE] [%u.%03u.%03u] Level:%d Tag:%s Msg:%s\n", 
           record->time_sec, record->time_ms, record->time_us,
           record->level, record->tag, record->payload);
}

static log_backend_t g_console_backend = {
    .name = "CONSOLE",
    .min_level = LOG_LEVEL_INFO,
    .enabled = true,
    .output = console_backend_output,
};

TEST_CASE(log_multi_backend_demo) {
    log_init();
    
    // Open files
    g_bin_file = fopen("test_log.bin", "wb");
    g_txt_file = fopen("test_log.txt", "w");
    
    TEST_ASSERT_NOT_NULL(g_bin_file);
    TEST_ASSERT_NOT_NULL(g_txt_file);

    // Register backends
    log_backend_register(&g_bin_backend);
    log_backend_register(&g_txt_backend);
    log_backend_register(&g_console_backend);
    
    printf("\n--- Starting Multi-Backend Log Test ---\n");

    // Write logs
    LOG_I("MAIN", "System starting...");
    LOG_W("SENSOR", "Temperature high: %d", 85);
    LOG_E("MOTOR", "Overcurrent detected!");
    LOG_D("DEBUG", "This should be ignored by default min_level=INFO");
    
    // Flush to process (Synchronous simulation)
    log_flush();
    
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
        LOG_I("ISR", "Interrupt event %d", count++);
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
    log_init();
    
    // Setup Mock Time Provider
    g_mock_ms = 0;
    g_mock_us = 0;
    g_time_thread_running = true;
    time_set_ms_provider(mock_get_time_ms);
    time_set_us_provider(mock_get_time_us);
    HANDLE hTimeThread = (HANDLE)_beginthreadex(NULL, 0, time_mock_thread, NULL, 0, NULL);
    
    // Initialize Async
    async_init(&g_test_async, g_async_mem, sizeof(g_async_mem), NULL);
    log_set_async(&g_test_async);
    
    // Register Console Backend
    log_backend_register(&g_console_backend);
    
    printf("\n--- Starting Async Multithread Test ---\n");
    
    // Start ISR thread
    g_thread_running = true;
    HANDLE hThread = (HANDLE)_beginthreadex(NULL, 0, isr_thread_func, NULL, 0, NULL);
    TEST_ASSERT_TRUE(hThread != NULL);
    
    // Main loop simulation for 500ms
    uint32_t start_time = GetTickCount();
    while (GetTickCount() - start_time < 500) {
        // Simulate main loop work
        LOG_D("MAIN", "Main loop working...");
        
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
static log_record_t g_last_record;

static void test_backend_output(log_backend_t* backend, const log_record_t* record) {
    (void)backend;
    g_last_record = *record;
    // Copy payload to buffer for verification
    size_t len = record->payload_len < sizeof(g_test_output_buf) ? record->payload_len : sizeof(g_test_output_buf) - 1;
    memcpy(g_test_output_buf, record->payload, len);
    g_test_output_buf[len] = '\0';
}

static log_backend_t g_test_backend = {
    .name = "TEST",
    .min_level = LOG_LEVEL_INFO,
    .enabled = true,
    .output = test_backend_output,
    .panic_output = NULL,
    .flush = NULL
};

TEST_CASE(log_tags_and_levels) {
    log_init();
    
    // Setup backend to capture output
    // We reuse g_test_backend which captures the last record
    // Backend allows everything down to VERBOSE so we can test the frontend filtering
    g_test_backend.min_level = LOG_LEVEL_VERBOSE; 
    log_backend_register(&g_test_backend);
    
    // Set Global Level to INFO
    log_set_level(LOG_LEVEL_INFO);

    // 1. Test Default Global Filter (INFO)
    // DEBUG should be filtered out
    memset(g_test_output_buf, 0, sizeof(g_test_output_buf));
    LOG_D("BLE", "BLE Debug");
    log_flush();
    TEST_ASSERT_EQUAL_STRING("", g_test_output_buf);

    // INFO should pass
    LOG_I("BLE", "BLE Info");
    log_flush();
    TEST_ASSERT_EQUAL_STRING("BLE Info", g_test_output_buf);

    // 2. Test Tag Specific Filter
    // Set "WIFI" to DEBUG (Allowing more verbose logs for WIFI)
    log_set_tag_level("WIFI", LOG_LEVEL_DEBUG);

    // WIFI DEBUG should now pass
    memset(g_test_output_buf, 0, sizeof(g_test_output_buf));
    int line = __LINE__ + 1;
    LOG_D("WIFI", "WIFI Debug Packet");
    log_flush();
    
    char expected[256];
    snprintf(expected, sizeof(expected), "[%s:%d] WIFI Debug Packet", __FILE__, line);
    TEST_ASSERT_EQUAL_STRING(expected, g_test_output_buf);
    TEST_ASSERT_EQUAL_STRING("WIFI", g_last_record.tag);

    // BLE DEBUG should still be filtered (Global is INFO)
    memset(g_test_output_buf, 0, sizeof(g_test_output_buf));
    LOG_D("BLE", "BLE Debug 2");
    log_flush();
    TEST_ASSERT_EQUAL_STRING("", g_test_output_buf);

    // 3. Test Tag Specific Filter (Restricting)
    // Set "NOISY" to ERROR (Only allow errors)
    log_set_tag_level("NOISY", LOG_LEVEL_ERROR);

    // NOISY INFO should be filtered (Global is INFO, but Tag is ERROR)
    memset(g_test_output_buf, 0, sizeof(g_test_output_buf));
    LOG_I("NOISY", "Noisy Info");
    log_flush();
    TEST_ASSERT_EQUAL_STRING("", g_test_output_buf);

    // NOISY ERROR should pass
    LOG_E("NOISY", "Noisy Error");
    log_flush();
    TEST_ASSERT_EQUAL_STRING("Noisy Error", g_test_output_buf);
    TEST_ASSERT_EQUAL_STRING("NOISY", g_last_record.tag);
}

TEST_CASE(log_trace_macro) {
    log_init();
    // Enable VERBOSE globally and for backend
    log_set_level(LOG_LEVEL_VERBOSE);
    g_test_backend.min_level = LOG_LEVEL_VERBOSE;
    log_backend_register(&g_test_backend);
    
    memset(g_test_output_buf, 0, sizeof(g_test_output_buf));
    
    int line = __LINE__ + 1;
    LOG_V("TRACE", "Variable x=%d", 42);
    log_flush();
    
    // Expected output: "[filename:line] Variable x=42"
    char expected[256];
    snprintf(expected, sizeof(expected), "[%s:%d] Variable x=%d", __FILE__, line, 42);
    
    TEST_ASSERT_EQUAL_STRING(expected, g_test_output_buf);
    TEST_ASSERT_EQUAL_STRING("TRACE", g_last_record.tag);
}

TEST_CASE(log_tags_console_demo) {
    log_init();
    log_backend_register(&g_console_backend);
    
    printf("\n--- Multi-Tag Console Demo ---\n");
    LOG_I("WIFI", "Connecting to AP...");
    LOG_I("TCP", "Handshake success");
    LOG_W("HEAP", "Memory low: %d bytes free", 1024);
    LOG_E("FLASH", "Write error at 0x08000000");
    
    // Debug log with file/line
    LOG_D("ASSERT", "Pointer is NULL (Debug info)");
    
    log_flush();
    printf("--- End Demo ---\n");
}

TEST_CASE(log_all_levels_console) {
    log_init();
    
    // Allow all levels for console backend
    g_console_backend.min_level = LOG_LEVEL_VERBOSE;
    log_backend_register(&g_console_backend);
    
    // Allow all levels globally
    log_set_level(LOG_LEVEL_VERBOSE);
    
    printf("\n--- All Levels Console Test ---\n");
    
    LOG_E("TEST", "This is an ERROR message");
    LOG_W("TEST", "This is a WARN message");
    LOG_I("TEST", "This is an INFO message");
    LOG_D("TEST", "This is a DEBUG message");
    LOG_V("TEST", "This is a VERBOSE message");
    
    log_flush();
    printf("--- End All Levels Test ---\n");
}

TEST_CASE(log_isr_deferred) {
    log_init();
    log_backend_register(&g_console_backend);
    
    printf("\n--- ISR Deferred Formatting Test ---\n");
    
    // Test Macros (Auto count)
    LOG_ISR_I("ISR", "ISR Enter Macro");
    LOG_ISR_I("ISR", "ADC Value: %d", 1024);
    LOG_ISR_W("ISR", "DMA Error: Ch%d Status=0x%x", 1, 0xFF);
    LOG_ISR_D("ISR", "Data: %02x %02x %02x %02x", 0xAA, 0xBB, 0xCC, 0xDD);
    LOG_ISR_I("ISR", "Many Args: %d %d %d %d %d", 1, 2, 3, 4, 5);
    
    log_flush();
    printf("--- End ISR Test ---\n");
}

// --- Merged Test from test_em_log_isr.c ---

// --- Mock Time for ISR Test ---
static uint32_t isr_test_get_ms(void) { return 1000; }
static uint32_t isr_test_get_us(void) { return 1000000 + 500; }

// --- Mock Backend for ISR Test ---
static char g_isr_test_output[1024];
static void isr_test_output(log_backend_t* backend, const log_record_t* record) {
    (void)backend;
    // Format: [LEVEL] TAG: Payload
    snprintf(g_isr_test_output, sizeof(g_isr_test_output), "[%d] %s: %.*s", 
        record->level, record->tag, (int)record->payload_len, (const char*)record->payload);
    printf("Backend Output: %s\n", g_isr_test_output);
}

static log_backend_t g_isr_test_backend = {
    .output = isr_test_output,
    .enabled = true,
    .min_level = LOG_LEVEL_VERBOSE
};

static uint8_t g_isr_async_buf[1024];
static async_t g_isr_async;

TEST_CASE(log_isr_verification_logic) {
    printf("Starting ISR Verification Test...\n");

    // 1. Setup Time
    time_set_ms_provider(isr_test_get_ms);
    time_set_us_provider(isr_test_get_us);

    // 2. Setup Async
    async_init(&g_isr_async, g_isr_async_buf, sizeof(g_isr_async_buf), NULL);

    // 3. Setup Log
    log_init();
    log_set_async(&g_isr_async);
    log_backend_register(&g_isr_test_backend);
    log_set_level(LOG_LEVEL_VERBOSE);

    // 4. Test Case 1: Simple ISR Log
    printf("\nTest 1: Simple ISR Log\n");
    memset(g_isr_test_output, 0, sizeof(g_isr_test_output));
    LOG_ISR_I("ISR_TEST", "Hello ISR");
    
    // Process Async
    async_process(&g_isr_async, 0);
    
    if (strstr(g_isr_test_output, "Hello ISR")) {
        printf("PASS: Found 'Hello ISR'\n");
    } else {
        printf("FAIL: Expected 'Hello ISR', got '%s'\n", g_isr_test_output);
    }

    // 5. Test Case 2: ISR Log with Args
    printf("\nTest 2: ISR Log with Args\n");
    memset(g_isr_test_output, 0, sizeof(g_isr_test_output));
    int val1 = 42;
    LOG_ISR_I("ISR_TEST", "Value=%d", val1);
    
    async_process(&g_isr_async, 0);
    
    if (strstr(g_isr_test_output, "Value=42")) {
        printf("PASS: Found 'Value=42'\n");
    } else {
        printf("FAIL: Expected 'Value=42', got '%s'\n", g_isr_test_output);
    }

    // 6. Test Case 3: ISR Log with Multiple Args
    printf("\nTest 3: ISR Log with Multiple Args\n");
    memset(g_isr_test_output, 0, sizeof(g_isr_test_output));
    LOG_ISR_W("ISR_TEST", "A=%d, B=%d, C=%d", 10, 20, 30);
    
    async_process(&g_isr_async, 0);
    
    if (strstr(g_isr_test_output, "A=10, B=20, C=30")) {
        printf("PASS: Found 'A=10, B=20, C=30'\n");
    } else {
        printf("FAIL: Expected 'A=10, B=20, C=30', got '%s'\n", g_isr_test_output);
    }

    // 7. Test Case 4: ISR Log with String Pointer
    printf("\nTest 4: ISR Log with String Pointer\n");
    memset(g_isr_test_output, 0, sizeof(g_isr_test_output));
    LOG_ISR_E("ISR_TEST", "Status=%s", "ERROR");
    
    async_process(&g_isr_async, 0);
    
    if (strstr(g_isr_test_output, "Status=ERROR")) {
        printf("PASS: Found 'Status=ERROR'\n");
    } else {
        printf("FAIL: Expected 'Status=ERROR', got '%s'\n", g_isr_test_output);
    }

    return 0;
}

#endif

