#include "compiler_compat.h"

#if TEST_ENABLE
#include <em_test/test.h>
#include "datatype.h"

// 测试 CA_ALIGNED
CA_ALIGNED(16) static char s_compat_test_aligned_buf[32];
typedef struct {
    char a;
    int b;
} CA_ALIGNED(8) compat_test_aligned_t;

// 测试 CA_PACKED
typedef struct {
    char a;
    int b;  // 正常情况下会对齐到 4 字节，所以偏移量是 4。打包后：偏移量应为 1。
} CA_PACKED compat_test_packed_t;

// 测试 CA_INLINE
CA_INLINE int compat_test_inline_add(int a, int b) {
    return a + b;
}

// 测试 CA_WEAK
CA_WEAK int compat_test_weak_func(void) {
    return 1;
}

TEST_CASE(test_compiler_compat_macros) {
    // 1. 检查 CA_ALIGNED
    TEST_ASSERT_EQUAL_INT(0, (i32)s_compat_test_aligned_buf % 16);
    // 注意：sizeof(aligned_struct_t) 应该是 8 的倍数
    TEST_ASSERT_EQUAL_INT(0, sizeof(compat_test_aligned_t) % 8);

    // 2. 检查 CA_PACKED
    // char(1) + int(4) = 5 字节 (如果打包生效)。如果不生效，可能是 8 字节。
    // MSVC 默认不支持通过 attribute/macro 后缀方式打包，CA_PACKED 为空
#if !defined(_MSC_VER)
    TEST_ASSERT_EQUAL_INT(5, sizeof(compat_test_packed_t));
#endif

    // 3. 检查 CA_INLINE 执行
    TEST_ASSERT_EQUAL_INT(3, compat_test_inline_add(1, 2));

    // 4. 检查 CA_WEAK 执行
    TEST_ASSERT_EQUAL_INT(1, compat_test_weak_func());
}

#endif
