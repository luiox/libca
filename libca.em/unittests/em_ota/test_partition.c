/* Auto-migrated from src/em_ota/partition.c test blocks */
#include "partition.h"
#include <em_base/debug.h>
#include <em_base/string_util.h>


#include <em_test/test.h>
#include <em_base/memory_util.h>

/* 模拟 Flash：256KB */
#define MOCK_FLASH_SIZE (256 * 1024)
static u8 g_mock_flash[MOCK_FLASH_SIZE];

/* 模拟 Flash 基地址 */
#define MOCK_FLASH_BASE 0x08000000

/* 模拟擦除值（通常是 0xFF） */
#define MOCK_ERASE_VALUE 0xFF

/* 测试用分区表（适配 256KB 模拟 Flash） */
static const partition_t test_partitions[] = {
    {"bootloader", 0x08000000, 0x00004000, PARTITION_FLAG_READONLY}, /* 16KB */
    {"app",
     0x08004000,
     0x00020000,
     PARTITION_FLAG_READABLE | PARTITION_FLAG_WRITABLE | PARTITION_FLAG_ERASEABLE}, /* 128KB */
    {"download",
     0x08024000,
     0x00010000,
     PARTITION_FLAG_READABLE | PARTITION_FLAG_WRITABLE | PARTITION_FLAG_ERASEABLE}, /* 64KB */
    {"params",
     0x08034000,
     0x00004000,
     PARTITION_FLAG_READABLE | PARTITION_FLAG_WRITABLE | PARTITION_FLAG_ERASEABLE}, /* 16KB */
};

/* 模拟读取 */
static i32 mock_read(u32 addr, u8* buf, u32 len)
{
    u32 offset = addr - MOCK_FLASH_BASE;
    if (offset + len > MOCK_FLASH_SIZE) {
        return -1;
    }
    mem_cpy(buf, &g_mock_flash[offset], len);
    return 0;
}

/* 模拟写入 */
static i32 mock_write(u32 addr, const u8* data, u32 len)
{
    u32 offset = addr - MOCK_FLASH_BASE;
    if (offset + len > MOCK_FLASH_SIZE) {
        return -1;
    }
    mem_cpy(&g_mock_flash[offset], data, len);
    return 0;
}

/* 模拟擦除 */
static i32 mock_erase(u32 addr, u32 len)
{
    u32 offset = addr - MOCK_FLASH_BASE;
    if (offset + len > MOCK_FLASH_SIZE) {
        return -1;
    }
    mem_set(&g_mock_flash[offset], MOCK_ERASE_VALUE, len);
    return 0;
}

static const partition_port_t mock_port = {
    .read  = mock_read,
    .write = mock_write,
    .erase = mock_erase,
};

static void test_setup_port(void)
{
    partition_register_port(&mock_port);
}

/* 测试计数器 */
static u32 g_callback_count;
static u32 g_callback_last_offset;
static u32 g_callback_last_len;

static void test_callback(u32 offset, const u8* data, u32 len, void* userdata)
{
    g_callback_count++;
    g_callback_last_offset = offset;
    g_callback_last_len    = len;
    unused_param(data);
    unused_param(userdata);
}

TEST_CASE(partition_port_register)
{
    partition_register_port(NULL);
    TEST_ASSERT(!partition_port_is_registered());
    partition_register_port(&mock_port);
    TEST_ASSERT(partition_port_is_registered());
}

TEST_CASE(partition_find_test)
{
    usize count = sizeof(test_partitions) / sizeof(test_partitions[0]);

    const partition_t* p = partition_find(test_partitions, count, "app");
    TEST_ASSERT_NOT_NULL(p);
    TEST_ASSERT_EQUAL_STRING("app", p->name);
    TEST_ASSERT_EQUAL_UINT(0x08004000, p->start);
    TEST_ASSERT_EQUAL_UINT(0x00020000, p->size);

    p = partition_find(test_partitions, count, "nonexistent");
    TEST_ASSERT_NULL(p);

    p = partition_find(test_partitions, count, NULL);
    TEST_ASSERT_NULL(p);

    p = partition_find(NULL, count, "app");
    TEST_ASSERT_NULL(p);
}

TEST_CASE(partition_read_write_basic)
{
    test_setup_port();

    /* 先擦除整个模拟 Flash */
    mem_set(g_mock_flash, MOCK_ERASE_VALUE, MOCK_FLASH_SIZE);

    usize              count = sizeof(test_partitions) / sizeof(test_partitions[0]);
    const partition_t* app   = partition_find(test_partitions, count, "app");
    TEST_ASSERT_NOT_NULL(app);

    /* 写入数据 */
    u8  write_data[] = {0x01, 0x02, 0x03, 0x04, 0x05};
    i32 ret          = partition_write(app, 0, write_data, sizeof(write_data));
    TEST_ASSERT_EQUAL_INT(PARTITION_OK, ret);

    /* 读取验证 */
    u8 read_buf[8] = {0};
    ret            = partition_read(app, 0, read_buf, sizeof(write_data));
    TEST_ASSERT_EQUAL_INT(PARTITION_OK, ret);
    TEST_ASSERT_EQUAL_MEMORY(write_data, read_buf, sizeof(write_data));

    /* 偏移读写 */
    ret = partition_write(app, 100, write_data, sizeof(write_data));
    TEST_ASSERT_EQUAL_INT(PARTITION_OK, ret);

    ret = partition_read(app, 100, read_buf, sizeof(write_data));
    TEST_ASSERT_EQUAL_INT(PARTITION_OK, ret);
    TEST_ASSERT_EQUAL_MEMORY(write_data, read_buf, sizeof(write_data));
}

TEST_CASE(partition_readonly_check)
{
    test_setup_port();

    mem_set(g_mock_flash, MOCK_ERASE_VALUE, MOCK_FLASH_SIZE);

    usize              count = sizeof(test_partitions) / sizeof(test_partitions[0]);
    const partition_t* boot  = partition_find(test_partitions, count, "bootloader");
    TEST_ASSERT_NOT_NULL(boot);

    u8  data[] = {0xAA, 0xBB};
    i32 ret    = partition_write(boot, 0, data, sizeof(data));
    TEST_ASSERT_EQUAL_INT(PARTITION_ERR_READONLY, ret);

    ret = partition_erase(boot);
    TEST_ASSERT_EQUAL_INT(PARTITION_ERR_READONLY, ret);

    /* 只读分区应该可以读取 */
    ret = partition_read(boot, 0, data, sizeof(data));
    TEST_ASSERT_EQUAL_INT(PARTITION_OK, ret);
}

TEST_CASE(partition_out_of_range)
{
    test_setup_port();

    mem_set(g_mock_flash, MOCK_ERASE_VALUE, MOCK_FLASH_SIZE);

    usize              count    = sizeof(test_partitions) / sizeof(test_partitions[0]);
    const partition_t* download = partition_find(test_partitions, count, "download");
    TEST_ASSERT_NOT_NULL(download);

    u8 data[16] = {0};
    /* 超出分区范围 */
    i32 ret = partition_write(download, download->size - 8, data, 16);
    TEST_ASSERT_EQUAL_INT(PARTITION_ERR_OUT_OF_RANGE, ret);

    ret = partition_read(download, download->size - 8, data, 16);
    TEST_ASSERT_EQUAL_INT(PARTITION_ERR_OUT_OF_RANGE, ret);

    /* 边界测试：刚好在范围内 */
    ret = partition_write(download, download->size - 16, data, 16);
    TEST_ASSERT_EQUAL_INT(PARTITION_OK, ret);
}

TEST_CASE(partition_erase_test)
{
    test_setup_port();

    /* 先写入一些数据 */
    mem_set(g_mock_flash, 0x00, MOCK_FLASH_SIZE);

    usize              count  = sizeof(test_partitions) / sizeof(test_partitions[0]);
    const partition_t* params = partition_find(test_partitions, count, "params");
    TEST_ASSERT_NOT_NULL(params);

    /* 擦除分区 */
    i32 ret = partition_erase(params);
    TEST_ASSERT_EQUAL_INT(PARTITION_OK, ret);

    /* 验证擦除后数据为 0xFF */
    u8 buf[16];
    ret = partition_read(params, 0, buf, sizeof(buf));
    TEST_ASSERT_EQUAL_INT(PARTITION_OK, ret);

    for (usize i = 0; i < sizeof(buf); i++) {
        TEST_ASSERT_EQUAL_UINT(MOCK_ERASE_VALUE, buf[i]);
    }
}

TEST_CASE(partition_stream_basic)
{
    test_setup_port();

    mem_set(g_mock_flash, MOCK_ERASE_VALUE, MOCK_FLASH_SIZE);

    usize              count    = sizeof(test_partitions) / sizeof(test_partitions[0]);
    const partition_t* download = partition_find(test_partitions, count, "download");
    TEST_ASSERT_NOT_NULL(download);

    partition_stream_t stream;
    u32                total_size = 1024;
    i32                ret        = partition_stream_open(&stream, download, total_size);
    TEST_ASSERT_EQUAL_INT(PARTITION_OK, ret);
    TEST_ASSERT_EQUAL_UINT(0, partition_stream_offset(&stream));
    TEST_ASSERT_EQUAL_UINT(total_size, partition_stream_remaining(&stream));

    /* 分块写入 */
    u8 block1[256];
    u8 block2[256];
    u8 block3[512];
    for (usize i = 0; i < 256; i++) {
        block1[i] = (u8)i;
        block2[i] = (u8)(i + 1);
    }
    for (usize i = 0; i < 512; i++) {
        block3[i] = (u8)(i + 2);
    }

    ret = partition_stream_write(&stream, block1, sizeof(block1));
    TEST_ASSERT_EQUAL_INT(PARTITION_OK, ret);
    TEST_ASSERT_EQUAL_UINT(256, partition_stream_offset(&stream));
    TEST_ASSERT_EQUAL_UINT(768, partition_stream_remaining(&stream));

    ret = partition_stream_write(&stream, block2, sizeof(block2));
    TEST_ASSERT_EQUAL_INT(PARTITION_OK, ret);

    ret = partition_stream_write(&stream, block3, sizeof(block3));
    TEST_ASSERT_EQUAL_INT(PARTITION_OK, ret);
    TEST_ASSERT_EQUAL_UINT(total_size, partition_stream_offset(&stream));
    TEST_ASSERT_EQUAL_UINT(0, partition_stream_remaining(&stream));

    /* 关闭流 */
    ret = partition_stream_close(&stream);
    TEST_ASSERT_EQUAL_INT(PARTITION_OK, ret);

    /* 验证数据 */
    u8 verify[1024];
    ret = partition_read(download, 0, verify, sizeof(verify));
    TEST_ASSERT_EQUAL_INT(PARTITION_OK, ret);
    TEST_ASSERT_EQUAL_MEMORY(block1, verify, 256);
    TEST_ASSERT_EQUAL_MEMORY(block2, verify + 256, 256);
    TEST_ASSERT_EQUAL_MEMORY(block3, verify + 512, 512);
}

TEST_CASE(partition_stream_callback)
{
    test_setup_port();

    mem_set(g_mock_flash, MOCK_ERASE_VALUE, MOCK_FLASH_SIZE);

    usize              count    = sizeof(test_partitions) / sizeof(test_partitions[0]);
    const partition_t* download = partition_find(test_partitions, count, "download");
    TEST_ASSERT_NOT_NULL(download);

    g_callback_count = 0;

    partition_stream_t stream;
    i32                ret = partition_stream_open(&stream, download, 100);
    TEST_ASSERT_EQUAL_INT(PARTITION_OK, ret);

    /* 设置回调 */
    stream.on_block_written = test_callback;

    u8 data[50];
    for (usize i = 0; i < 50; i++) {
        data[i] = (u8)i;
    }

    /* 写入两块 */
    ret = partition_stream_write(&stream, data, 50);
    TEST_ASSERT_EQUAL_INT(PARTITION_OK, ret);
    TEST_ASSERT_EQUAL_UINT(1, g_callback_count);
    TEST_ASSERT_EQUAL_UINT(0, g_callback_last_offset);
    TEST_ASSERT_EQUAL_UINT(50, g_callback_last_len);

    ret = partition_stream_write(&stream, data, 50);
    TEST_ASSERT_EQUAL_INT(PARTITION_OK, ret);
    TEST_ASSERT_EQUAL_UINT(2, g_callback_count);
    TEST_ASSERT_EQUAL_UINT(50, g_callback_last_offset);
    TEST_ASSERT_EQUAL_UINT(50, g_callback_last_len);

    ret = partition_stream_close(&stream);
    TEST_ASSERT_EQUAL_INT(PARTITION_OK, ret);
}

TEST_CASE(partition_stream_size_mismatch)
{
    test_setup_port();

    mem_set(g_mock_flash, MOCK_ERASE_VALUE, MOCK_FLASH_SIZE);

    usize              count    = sizeof(test_partitions) / sizeof(test_partitions[0]);
    const partition_t* download = partition_find(test_partitions, count, "download");
    TEST_ASSERT_NOT_NULL(download);

    partition_stream_t stream;
    i32                ret = partition_stream_open(&stream, download, 100);
    TEST_ASSERT_EQUAL_INT(PARTITION_OK, ret);

    /* 只写入 50 字节，但预期 100 字节 */
    u8 data[50] = {0};
    ret         = partition_stream_write(&stream, data, 50);
    TEST_ASSERT_EQUAL_INT(PARTITION_OK, ret);

    /* 关闭时应返回大小不匹配错误 */
    ret = partition_stream_close(&stream);
    TEST_ASSERT_EQUAL_INT(PARTITION_ERR_SIZE_MISMATCH, ret);
}

TEST_CASE(partition_stream_exceed_size)
{
    test_setup_port();

    mem_set(g_mock_flash, MOCK_ERASE_VALUE, MOCK_FLASH_SIZE);

    usize              count    = sizeof(test_partitions) / sizeof(test_partitions[0]);
    const partition_t* download = partition_find(test_partitions, count, "download");
    TEST_ASSERT_NOT_NULL(download);

    partition_stream_t stream;
    i32                ret = partition_stream_open(&stream, download, 100);
    TEST_ASSERT_EQUAL_INT(PARTITION_OK, ret);

    u8 data[200] = {0};

    /* 尝试写入超过预期总大小 */
    ret = partition_stream_write(&stream, data, 200);
    TEST_ASSERT_EQUAL_INT(PARTITION_ERR_OUT_OF_RANGE, ret);

    /* 尝试打开超过分区大小 */
    partition_stream_t stream2;
    ret = partition_stream_open(&stream2, download, download->size + 1);
    TEST_ASSERT_EQUAL_INT(PARTITION_ERR_OUT_OF_RANGE, ret);
}

TEST_CASE(partition_stream_not_open)
{
    partition_stream_t stream = {0};

    u8  data[10] = {0};
    i32 ret      = partition_stream_write(&stream, data, sizeof(data));
    TEST_ASSERT_EQUAL_INT(PARTITION_ERR_NOT_OPEN, ret);

    ret = partition_stream_close(&stream);
    TEST_ASSERT_EQUAL_INT(PARTITION_ERR_NOT_OPEN, ret);
}
