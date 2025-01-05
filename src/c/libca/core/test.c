#include "test.h"

#ifdef LIBCA_USE_TEST

// TEST_CASE(test_module)
// {
//     int result = 2 + 2;
//     TEST_ASSERT_EQUAL_INT(4, result);
// }

int total_tests  = 0;

 int passed_tests  = 0;

// 全局测试用例数组
 test_t* tests = NULL;

// 测试用例数量
 int num_tests = 0;
int cur_num = 0;


#endif // LIBCA_USE_TEST
