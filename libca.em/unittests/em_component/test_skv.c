/* Auto-migrated from src/em_component/skv.c test blocks */
#include "skv.h"
#include <em_util/crc.h>
#include <string.h>


#include <em_test/test.h>
#include <string.h>
#include <stdio.h>

FILE* g_test_skv_file = NULL;

static bool test_skv_read(u32 addr, u8* buf, u32 len)
{
    if (!g_test_skv_file)
        return false;
    if (fseek(g_test_skv_file, (long)addr, SEEK_SET) != 0)
        return false;
    size_t r = fread(buf, 1, (size_t)len, g_test_skv_file);
    return r == len;
}

static bool test_skv_write(u32 addr, u8* buf, u32 len)
{
    if (!g_test_skv_file)
        return false;
    if (fseek(g_test_skv_file, (long)addr, SEEK_SET) != 0)
        return false;
    size_t w = fwrite(buf, 1, (size_t)len, g_test_skv_file);
    fflush(g_test_skv_file);
    return w == len;
}

static skv_port_t test_skv_port = {
    .read  = test_skv_read,
    .write = test_skv_write,
};

TEST_CASE(skv_basic)
{
    // 使用一个临时二进制文件模拟eeprom
    g_test_skv_file = fopen("test_skv.bin", "wb+");
    TEST_ASSERT_NOT_NULL(g_test_skv_file);

    // 分配1KB作为模拟存储
    fseek(g_test_skv_file, 1024 - 1, SEEK_SET);
    fputc(0, g_test_skv_file);
    fflush(g_test_skv_file);

    skv_bind_port(&test_skv_port);

    skv_t skv;
    skv_init(&skv, 0);
    skv.total_size = 1024;
    skv.next_addr  = skv.start_addr + 32;
    TEST_ASSERT(skv_write_header(&skv));

    // 写入一个i32值
    i32 v = 0x12345678;
    TEST_ASSERT_EQUAL_INT(0, skv_put_i32(&skv, "mykey", v));

    // 确认header已持久化
    skv_t skv2;
    skv2.start_addr = 0;
    TEST_ASSERT(skv_read_header(&skv2));
    TEST_ASSERT_EQUAL_UINT(1u, skv2.num);

    // 读取回刚才的数据
    skv_kv_item_t q;
    memset(&q, 0, sizeof(q));
    q.key        = "mykey";
    q.key_length = (u8)strlen(q.key);
    i32 got      = 0;
    q.value      = &got;
    TEST_ASSERT_EQUAL_INT(0, skv_get_item(&skv, &q));
    TEST_ASSERT_EQUAL_UINT((unsigned int)SKV_TYPE_I32, (unsigned int)q.value_type);
    TEST_ASSERT_EQUAL_INT(v, got);

    // 未找到key
    q.key        = "nokey";
    q.key_length = (u8)strlen(q.key);
    TEST_ASSERT_EQUAL_INT(SKV_ERR_NOT_FOUND, skv_get_item(&skv, &q));

    fclose(g_test_skv_file);
    g_test_skv_file = NULL;
    remove("test_skv.bin");
}

TEST_CASE(skv_types_put_get)
{
    // 使用一个临时二进制文件模拟eeprom
    g_test_skv_file = fopen("test_skv_types_put_get.bin", "wb+");
    TEST_ASSERT_NOT_NULL(g_test_skv_file);

    // 分配2KB作为模拟存储
    fseek(g_test_skv_file, 2048 - 1, SEEK_SET);
    fputc(0, g_test_skv_file);
    fflush(g_test_skv_file);

    skv_bind_port(&test_skv_port);

    skv_t skv;
    skv_init(&skv, 0);
    skv.total_size = 2048;
    skv.next_addr  = skv.start_addr + 32;
    TEST_ASSERT(skv_write_header(&skv));

    // 写入各种类型
    TEST_ASSERT_EQUAL_INT(0, skv_put_u8(&skv, "u8", 0xAB));
    TEST_ASSERT_EQUAL_INT(0, skv_put_u16(&skv, "u16", 0x1234));
    TEST_ASSERT_EQUAL_INT(0, skv_put_u32(&skv, "u32", 0x89ABCDEF));
#ifdef HAS_INT64
    TEST_ASSERT_EQUAL_INT(0, skv_put_u64(&skv, "u64", (u64)0x1122334455667788ULL));
#endif
    TEST_ASSERT_EQUAL_INT(0, skv_put_i8(&skv, "i8", (i8)-5));
    TEST_ASSERT_EQUAL_INT(0, skv_put_i16(&skv, "i16", (i16)-12345));
    TEST_ASSERT_EQUAL_INT(0, skv_put_i32(&skv, "i32", (i32)-123456789));
    TEST_ASSERT_EQUAL_INT(0, skv_put_f32(&skv, "f32", (f32)3.14159f));
    TEST_ASSERT_EQUAL_INT(0, skv_put_f64(&skv, "f64", (f64)2.718281828459045));
    TEST_ASSERT_EQUAL_INT(0, skv_put_string(&skv, "str", "hello world"));
    const u8 blob_data[] = {0x01, 0x02, 0xFF, 0x00};
    TEST_ASSERT_EQUAL_INT(0, skv_put_blob(&skv, "blob", blob_data, sizeof(blob_data)));

    // 读取并验证
    u8 u8v;
    TEST_ASSERT_EQUAL_INT(0, skv_get_u8(&skv, "u8", &u8v));
    TEST_ASSERT_EQUAL_UINT(0xABu, (unsigned int)u8v);
    u16 u16v;
    TEST_ASSERT_EQUAL_INT(0, skv_get_u16(&skv, "u16", &u16v));
    TEST_ASSERT_EQUAL_UINT(0x1234u, (unsigned int)u16v);
    u32 u32v;
    TEST_ASSERT_EQUAL_INT(0, skv_get_u32(&skv, "u32", &u32v));
    TEST_ASSERT_EQUAL_UINT(0x89ABCDEFu, (unsigned int)u32v);
#ifdef HAS_INT64
    u64 u64v;
    TEST_ASSERT_EQUAL_INT(0, skv_get_u64(&skv, "u64", &u64v));
    TEST_ASSERT_EQUAL_UINT(0x1122334455667788ULL, u64v);
#endif
    i8 i8v;
    TEST_ASSERT_EQUAL_INT(0, skv_get_i8(&skv, "i8", &i8v));
    TEST_ASSERT_EQUAL_INT(-5, (int)i8v);
    i16 i16v;
    TEST_ASSERT_EQUAL_INT(0, skv_get_i16(&skv, "i16", &i16v));
    TEST_ASSERT_EQUAL_INT(-12345, (int)i16v);
    i32 i32v;
    TEST_ASSERT_EQUAL_INT(0, skv_get_i32(&skv, "i32", &i32v));
    TEST_ASSERT_EQUAL_INT(-123456789, (int)i32v);
    f32 f32v;
    TEST_ASSERT_EQUAL_INT(0, skv_get_f32(&skv, "f32", &f32v));
    TEST_ASSERT_EQUAL_FLOAT(3.14159f, f32v);
    f64 f64v;
    TEST_ASSERT_EQUAL_INT(0, skv_get_f64(&skv, "f64", &f64v));
    TEST_ASSERT_EQUAL_FLOAT(2.718281828459045, f64v);
    char strbuf[64];
    TEST_ASSERT_EQUAL_INT(0, skv_get_string(&skv, "str", strbuf, sizeof(strbuf)));
    TEST_ASSERT_EQUAL_STRING("hello world", strbuf);
    u8 out_blob[16];
    u8 out_len = 0;
    TEST_ASSERT_EQUAL_INT(0, skv_get_blob(&skv, "blob", out_blob, sizeof(out_blob), &out_len));
    TEST_ASSERT_EQUAL_UINT(sizeof(blob_data), out_len);
    TEST_ASSERT_EQUAL_MEMORY(blob_data, out_blob, out_len);

    fclose(g_test_skv_file);
    g_test_skv_file = NULL;
    remove("test_skv_types_put_get.bin");
}

TEST_CASE(skv_or_default_missing)
{
    g_test_skv_file = fopen("test_skv_or_default_missing.bin", "wb+");
    TEST_ASSERT_NOT_NULL(g_test_skv_file);
    fseek(g_test_skv_file, 1024 - 1, SEEK_SET);
    fputc(0, g_test_skv_file);
    fflush(g_test_skv_file);

    skv_bind_port(&test_skv_port);
    skv_t skv;
    skv_init(&skv, 0);
    skv.total_size = 1024;
    skv.next_addr  = skv.start_addr + 32;
    TEST_ASSERT(skv_write_header(&skv));

    u32 dflt = 0xDEADBEEFu;
    u32 got  = 0;
    TEST_ASSERT_EQUAL_INT(SKV_RET_DEFAULT_WRITTEN_NOT_FOUND,
                          skv_get_u32_or_default(&skv, "missing_u32", &got, dflt));
    TEST_ASSERT_EQUAL_UINT(dflt, got);

    // ensure it's persisted
    skv_t skv2;
    skv2.start_addr = 0;
    TEST_ASSERT(skv_read_header(&skv2));
    TEST_ASSERT_EQUAL_UINT(1u, skv2.num);

    fclose(g_test_skv_file);
    g_test_skv_file = NULL;
    remove("test_skv_or_default_missing.bin");
}

TEST_CASE(skv_or_default_write_failure)
{
    g_test_skv_file = fopen("test_skv_or_default_write_failure.bin", "wb+");
    TEST_ASSERT_NOT_NULL(g_test_skv_file);
    fseek(g_test_skv_file, 1024 - 1, SEEK_SET);
    fputc(0, g_test_skv_file);
    fflush(g_test_skv_file);

    skv_bind_port(&test_skv_port);
    skv_t skv;
    skv_init(&skv, 0);
    skv.total_size = 1024;
    skv.next_addr  = skv.start_addr + 32;
    TEST_ASSERT(skv_write_header(&skv));

    // simulate write failure by nulling the file pointer used by the port
    g_test_skv_file = NULL;
    u8 val          = 0;
    TEST_ASSERT_EQUAL_INT(SKV_ERR_WRITE_FAILED, skv_get_u8_or_default(&skv, "any", &val, 0x7F));

    // reopen and cleanup
    g_test_skv_file = fopen("test_skv_or_default_write_failure.bin", "rb+");
    TEST_ASSERT_NOT_NULL(g_test_skv_file);
    fclose(g_test_skv_file);
    g_test_skv_file = NULL;
    remove("test_skv_or_default_write_failure.bin");
}
