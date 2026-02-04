#include "mem_view.h"

#if TEST_ENABLE

#include "../em_test/test.h"

/*
 * 单元测试：mem_view 模块
 * - 测试风格：每个测试点为一个原子 TEST_CASE
 */

TEST_CASE(mem_view_init_and_remain) {
    u8 buf[4] = {0x11, 0x22, 0x33, 0x44};
    mem_view_t self;

    mem_view_init(&self, buf, sizeof(buf));
    TEST_ASSERT_EQUAL_UINT(4, mem_view_remain(&self));
}

TEST_CASE(mem_view_read_safe_basic) {
    u8 buf[4] = {0x01, 0x02, 0x03, 0x04};
    mem_view_t self;
    u8 a;
    u16 b;

    mem_view_init(&self, buf, sizeof(buf));

    TEST_ASSERT_TRUE(mem_view_read_u8_safe(&self, &a));
    TEST_ASSERT_EQUAL_UINT(0x01, a);

    TEST_ASSERT_TRUE(mem_view_read_u16_safe(&self, &b));
    TEST_ASSERT_EQUAL_UINT((u16)0x0302, b); /* 小端: low=0x02, high=0x03 */

    TEST_ASSERT_TRUE(mem_view_read_u8_safe(&self, &a));
    TEST_ASSERT_EQUAL_UINT(0x04, a);

    TEST_ASSERT_EQUAL_UINT(0, mem_view_remain(&self));
}

TEST_CASE(mem_view_read_safe_insufficient) {
    u8 buf[1] = {0x01};
    mem_view_t self;
    u16 out;

    mem_view_init(&self, buf, sizeof(buf));
    TEST_ASSERT_FALSE(mem_view_read_u16_safe(&self, &out));
    TEST_ASSERT_EQUAL_UINT(1, mem_view_remain(&self)); /* 游标不应被移动 */
}

TEST_CASE(mem_view_peek_safe_and_not_advance) {
    u8 buf[3] = {0xAA, 0xBB, 0xCC};
    mem_view_t self;
    u16 out;

    mem_view_init(&self, buf, sizeof(buf));
    TEST_ASSERT_TRUE(mem_view_peek_u16_safe(&self, 0, &out));
    TEST_ASSERT_EQUAL_UINT((u16)0xBBAA, out); /* 小端 */
    TEST_ASSERT_EQUAL_UINT(3, mem_view_remain(&self)); /* 未移动游标 */
}

TEST_CASE(mem_view_skip_behavior) {
    u8 buf[3] = {0x1, 0x2, 0x3};
    mem_view_t self;

    mem_view_init(&self, buf, sizeof(buf));

    TEST_ASSERT_TRUE(mem_view_skip(&self, 2));
    TEST_ASSERT_EQUAL_UINT(1, mem_view_remain(&self));

    TEST_ASSERT_FALSE(mem_view_skip(&self, 2)); /* 不足时返回 false，游标不变 */
    TEST_ASSERT_EQUAL_UINT(1, mem_view_remain(&self));
}

TEST_CASE(mem_view_read_buf_safe_behavior) {
    u8 buf[4] = {0x10, 0x20, 0x30, 0x40};
    mem_view_t self;
    u8 dst[3];

    mem_view_init(&self, buf, sizeof(buf));

    TEST_ASSERT_TRUE(mem_view_read_buf_safe(&self, dst, 3));
    TEST_ASSERT_EQUAL_MEMORY(buf, dst, 3);
    TEST_ASSERT_EQUAL_UINT(1, mem_view_remain(&self));

    /* 尝试读取超出范围的数据，失败且游标不变 */
    TEST_ASSERT_FALSE(mem_view_read_buf_safe(&self, dst, 2));
    TEST_ASSERT_EQUAL_UINT(1, mem_view_remain(&self));
}

TEST_CASE(mem_view_endianness_be) {
    u8 buf[4] = {0x01, 0x02, 0x03, 0x04};
    mem_view_t self;
    u16 a;
    u32 b;

    mem_view_init(&self, buf, sizeof(buf));

    TEST_ASSERT_EQUAL_UINT( (u16)0x0102, mem_view_read_u16_be(&self) );
    /* 还剩两字节，使用大端安全读取 */
    TEST_ASSERT_TRUE(mem_view_read_u16_be_safe(&self, &a));
    TEST_ASSERT_EQUAL_UINT((u16)0x0304, a);

    /* 重新初始化并测试 be 32 */
    mem_view_init(&self, buf, sizeof(buf));
    TEST_ASSERT_EQUAL_UINT( (u32)0x01020304, mem_view_read_u32_be(&self) );
}

TEST_CASE(mem_view_unsafe_reads) {
    u8 buf[6] = {0x01, 0x02, 0x03, 0x04, 0x05, 0x06};
    mem_view_t self;

    mem_view_init(&self, buf, sizeof(buf));

    /* 使用 unsafe 接口，在有足够数据的情况下也应得到正确值 */
    TEST_ASSERT_EQUAL_UINT( (u8)0x01, mem_view_read_u8(&self) );
    TEST_ASSERT_EQUAL_UINT( (u16)0x0302, mem_view_read_u16(&self) );
    /* 当前游标指向第三个索引，读取应为 0x04 */
    TEST_ASSERT_EQUAL_UINT( (u8)0x04, mem_view_read_u8(&self) );
    /* 已消耗 1 + 2 + 1 = 4 字节，剩余 2 字节 */
    TEST_ASSERT_EQUAL_UINT(2, mem_view_remain(&self));
}

TEST_CASE(mem_view_peek_u32_safe_bounds) {
    u8 buf[3] = {0x1, 0x2, 0x3};
    mem_view_t self;
    u32 out;

    mem_view_init(&self, buf, sizeof(buf));
    TEST_ASSERT_FALSE(mem_view_peek_u32_safe(&self, 0, &out));
}

#endif /* TEST_ENABLE */
