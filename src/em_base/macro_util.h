/**
 * @file macro_util.h
 * @author canrad (1517807724@qq.com)
 * @brief 宏工具的封装
 * 注意，如果仅仅是用于应用层开发，建议不要使用这些宏，仅仅用于基础组件开发
 * @version 0.2
 * @date 2026-01-18
 * @update 2026-01-31 明确已有的宏，统一添加 CA_ 前缀
 *
 * @copyright Copyright (c) 2026
 * 
 */
#ifndef LIBCA_EM_BASE_MACRO_UTIL_H
#define LIBCA_EM_BASE_MACRO_UTIL_H 

#define CA_MAKE_STRING(a) #a

#ifdef __COUNTER__
#define CA_UNIQUE_ID __COUNTER__
#else
#define CA_UNIQUE_ID __LINE__
#endif

// 各种数量的连接宏
#define __CA_CONNECT2(__A, __B) __A##__B
#define __CA_CONNECT3(__A, __B, __C) __A##__B##__C
#define __CA_CONNECT4(__A, __B, __C, __D) __A##__B##__C##__D
#define __CA_CONNECT5(__A, __B, __C, __D, __E) __A##__B##__C##__D##__E
#define __CA_CONNECT6(__A, __B, __C, __D, __E, __F) __A##__B##__C##__D##__E##__F
#define __CA_CONNECT7(__A, __B, __C, __D, __E, __F, __G) \
  __A##__B##__C##__D##__E##__F##__G
#define __CA_CONNECT8(__A, __B, __C, __D, __E, __F, __G, __H) \
  __A##__B##__C##__D##__E##__F##__G##__H
#define __CA_CONNECT9(__A, __B, __C, __D, __E, __F, __G, __H, __I) \
  __A##__B##__C##__D##__E##__F##__G##__H##__I

#define CA_CONNECT2(__A, __B) __CA_CONNECT2(__A, __B)
#define CA_CONNECT3(__A, __B, __C) __CA_CONNECT3(__A, __B, __C)
#define CA_CONNECT4(__A, __B, __C, __D) __CA_CONNECT4(__A, __B, __C, __D)
#define CA_CONNECT5(__A, __B, __C, __D, __E) __CA_CONNECT5(__A, __B, __C, __D, __E)
#define CA_CONNECT6(__A, __B, __C, __D, __E, __F) \
  __CA_CONNECT6(__A, __B, __C, __D, __E, __F)
#define CA_CONNECT7(__A, __B, __C, __D, __E, __F, __G) \
  __CA_CONNECT7(__A, __B, __C, __D, __E, __F, __G)
#define CA_CONNECT8(__A, __B, __C, __D, __E, __F, __G, __H) \
  __CA_CONNECT8(__A, __B, __C, __D, __E, __F, __G, __H)
#define CA_CONNECT9(__A, __B, __C, __D, __E, __F, __G, __H, __I) \
  __CA_CONNECT9(__A, __B, __C, __D, __E, __F, __G, __H, __I)


#define __CA_PLOOC_VA_NUM_ARGS_IMPL(_0, _1, _2, _3, _4, _5, _6, _7, _8, _9, _10, \
                                 _11, _12, _13, _14, _15, _16, __N, ...)      \
  __N

#define __CA_PLOOC_VA_NUM_ARGS(...)                                               \
  __CA_PLOOC_VA_NUM_ARGS_IMPL(0, ##__VA_ARGS__, 16, 15, 14, 13, 12, 11, 10, 9, 8, \
                           7, 6, 5, 4, 3, 2, 1, 0)

/**
 * @brief 获取可变参数个数
 */
#define CA_VA_NUM_ARGS(...) __CA_PLOOC_VA_NUM_ARGS(__VA_ARGS__)

/**
 * @brief 安全的局部标识符
 */
#define CA_SAFE_NAME(__NAME) CA_CONNECT3(__, __NAME, CA_UNIQUE_ID)

/**
 * @brief 连接宏
 */
#define CA_CONNECT(...) CA_CONNECT2(CA_CONNECT, CA_VA_NUM_ARGS(__VA_ARGS__))(__VA_ARGS__)

/**
 * @brief 选择宏, 根据参数个数N调用对应的__FUNC_N
 */
#define CA_EVAL(__FUNC, ...) CA_CONNECT2(__FUNC, CA_VA_NUM_ARGS(__VA_ARGS__))

#endif // !LIBCA_EM_BASE_MACRO_UTIL_H
