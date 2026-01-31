#include "macro_util.h"

// 暂时不加入标准的代码
#if 0
#define __USING_1(__declare) \
  for (__declare, *SAFE_NAME(using_ptr) = NULL; SAFE_NAME(using_ptr)++ == NULL;)

#define __USING_2(__declare, __on_leave_expr)   \
  for (__declare, *SAFE_NAME(using_ptr) = NULL; \
       SAFE_NAME(using_ptr)++ == NULL; __on_leave_expr)

#define __USING_3(__declare, __on_enter_expr, __on_leave_expr)      \
  for (__declare, *SAFE_NAME(using_ptr) = NULL;                     \
       SAFE_NAME(using_ptr)++ == NULL ? ((__on_enter_expr), 1) : 0; \
       __on_leave_expr)

#define __USING_4(__dcl1, __dcl2, __on_enter_expr, __on_leave_expr) \
  for (__dcl1, __dcl2, *SAFE_NAME(using_ptr) = NULL;                \
       SAFE_NAME(using_ptr)++ == NULL ? ((__on_enter_expr), 1) : 0; \
       (__on_leave_expr))

/**
 * @brief 局部变量
 * @param __declare 局部变量
 * @param __on_enter_expr 进入操作 [可选]
 * @param __on_leave_expr 离开操作 [可选]
 */
#define USING(...) EVAL(__USING_, __VA_ARGS__)(__VA_ARGS__)

// 编译时断言验证特定表达式。
// 如果表达式求值为零，编译将失败
// 只能用于整数类型
#if defined(__cplusplus) && __cplusplus >= 201103L
#define COMPILE_TIME_ASSERT(exp) static_assert(exp, #exp)
#elif defined(__STDC_VERSION__) && __STDC_VERSION__ >= 201112L
#define COMPILE_TIME_ASSERT(exp) _Static_assert(exp, #exp)
#else
#define COMPILE_TIME_ASSERT_IMPL(exp, random_variable_name) \
    typedef char random_variable_name[!(exp) ? -1 : 1]
#define COMPILE_TIME_ASSERT(exp) \
    COMPILE_TIME_ASSERT_IMPL(exp, SAFE_NAME(assert_var))
#endif
// demo
// void f()
// {
//   COMPILE_TIME_ASSERT(0==0);

// }

// 获取一个结构体的成员指针
// 例如
// struct list_head {
//     struct list_head *next, *prev;
// };
// struct task {
//     int id;
//     struct list_head node;
// };
// int main() {
//     struct task t = { .id = 42 };
//     struct list_head *nodeptr = &t.node;
//     struct task *tp = container_of(nodeptr, struct task, node);
//     printf("Task ID: %d\n", tp->id); // 输出: Task ID: 42
//     return 0;
// }
// 
#define container_of(ptr, type, member) \
    ((type *)((char *)(ptr) - offsetof(type, member)))
	

#    define ASSERT_STATIC_(condition) typedef char c_assert_##__LINE__[(condition) ? 1 : -1]

#    define ASSERT_STATIC(condition)            \
        struct c_assert_##__LINE__              \
        {                                       \
            unsigned int : (condition) ? 1 : 0; \
        }
#endif
