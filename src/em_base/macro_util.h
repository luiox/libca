/**
 * @file macro_util.h
 * @author canrad (1517807724@qq.com)
 * @brief 宏工具的封装
 * @version 0.2
 * @date 2026-01-18
 * @update 2026-01-31 明确已有的宏
 *
 * @copyright Copyright (c) 2026
 * 
 */
#ifndef LIBCA_EM_BASE_MACRO_UTIL_H
#define LIBCA_EM_BASE_MACRO_UTIL_H 

#define MAKE_STRING(a) #a
#ifdef __COUNTER__
#define UNIQUE_ID __COUNTER__
#else
#define UNIQUE_ID __LINE__
#endif

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
#define CONNECT9(__A, __B, __C, __D, __E, __F, __G, __H, __I) \
  __CONNECT9(__A, __B, __C, __D, __E, __F, __G, __H, __I)


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



#endif // !LIBCA_EM_BASE_MACRO_UTIL_H
