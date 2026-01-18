#ifndef LIBCA_EM_BASE_MACRO_UTIL_H
#define LIBCA_EM_BASE_MACRO_UTIL_H 

#define MAKE_STRING(a) #a
#define UNIQUE_ID __LINE__

// 各种数量的连接宏
#define __CONNECT2(__A, __B) __A##__B
#define __CONNECT3(__A, __B, __C) __A##__B##__C
#define __CONNECT4(__A, __B, __C, __D) __A##__B##__C##__D
#define __CONNECT5(__A, __B, __C, __D, __E) __A##__B##__C##__D##__E
#define __CONNECT6(__A, __B, __C, __D, __E, __F) __A##__B##__C##__D##__E##__F
#define __CONNECT7(__A, __B, __C, __D, __E, __F, __G) \
  __A##__B##__C##__D##__E##__F##__G
#define __CONNECT8(__A, __B, __C, __D, __E, __F, __G, __H) \
  __A##__B##__C##__D##__E##__F##__G##__H
#define __CONNECT9(__A, __B, __C, __D, __E, __F, __G, __H, __I) \
  __A##__B##__C##__D##__E##__F##__G##__H##__I
#define CONNECT2(__A, __B) __CONNECT2(__A, __B)
#define CONNECT3(__A, __B, __C) __CONNECT3(__A, __B, __C)
#define CONNECT4(__A, __B, __C, __D) __CONNECT4(__A, __B, __C, __D)
#define CONNECT5(__A, __B, __C, __D, __E) __CONNECT5(__A, __B, __C, __D, __E)
#define CONNECT6(__A, __B, __C, __D, __E, __F) \
  __CONNECT6(__A, __B, __C, __D, __E, __F)
#define CONNECT7(__A, __B, __C, __D, __E, __F, __G) \
  __CONNECT7(__A, __B, __C, __D, __E, __F, __G)
#define CONNECT8(__A, __B, __C, __D, __E, __F, __G, __H) \
  __CONNECT8(__A, __B, __C, __D, __E, __F, __G, __H)
#define CONNECT9(__A, __B, __C, __D, __E, __F, __G, __H, __I)


#define __PLOOC_VA_NUM_ARGS_IMPL(_0, _1, _2, _3, _4, _5, _6, _7, _8, _9, _10, \
                                 _11, _12, _13, _14, _15, _16, __N, ...)      \
  __N

#define __PLOOC_VA_NUM_ARGS(...)                                               \
  __PLOOC_VA_NUM_ARGS_IMPL(0, ##__VA_ARGS__, 16, 15, 14, 13, 12, 11, 10, 9, 8, \
                           7, 6, 5, 4, 3, 2, 1, 0)

/**
 * @brief 获取可变参数个数
 */
#define VA_NUM_ARGS(...) __PLOOC_VA_NUM_ARGS(__VA_ARGS__)

/**
 * @brief 安全的局部标识符
 */
#define SAFE_NAME(__NAME) CONNECT3(__, __NAME, UNIQUE_ID)

/**
 * @brief 连接宏
 */
#define CONNECT(...) CONNECT2(CONNECT, VA_NUM_ARGS(__VA_ARGS__))(__VA_ARGS__)

/**
 * @brief 选择宏, 根据参数个数N调用对应的__FUMC_N
 */
#define EVAL(__FUNC, ...) CONNECT2(__FUNC, VA_NUM_ARGS(__VA_ARGS__))

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
#define COMPILE_TIME_ASSERT_IMPL(exp, random_variable_name)                                                          \
    typedef char random_variable_name[!(exp) ? -1 : 1];

#define COMPILE_TIME_ASSERT(exp)                                                          \
    COMPILE_TIME_ASSERT_IMPL(exp, SAFE_NAME(assert_var))
// demo
// void f()
// {
//   COMPILE_TIME_ASSERT(0==0);

// }

#endif // !LIBCA_EM_BASE_MACRO_UTIL_H
