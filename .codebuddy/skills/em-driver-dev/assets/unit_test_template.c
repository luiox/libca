/* unit_test_template.c - 适用于无硬件依赖模块的单元测试样例 */

#if TEST_ENABLE
#include "../em_test/test.h"
#include "module_under_test.h"

TEST_CASE(module_feature_name) {
    // Setup

    // Action

    // Assert
    TEST_ASSERT_EQUAL_INT(expected, actual);
}
#endif
