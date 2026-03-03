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

// clang-format off

#define CA_MAKE_STRING(a) #a

/**
 * @brief 单行内唯一的ID
 * 
 */
#define CA_UNIQUE_ID __LINE__

/**
 * @brief 如果有__COUNTER__的情况下，做到真正的唯一ID，不会有局限于不同行
 * 
 */
#ifdef __COUNTER__
#define CA_REAL_UNIQUE_ID __COUNTER__
#else
#define CA_REAL_UNIQUE_ID CA_UNIQUE_ID
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


#define CA_EXPAND(x) x

#define __CA_PLOOC_VA_NUM_ARGS_IMPL(_0, _1, _2, _3, _4, _5, _6, _7, _8, _9, _10, \
                                 _11, _12, _13, _14, _15, _16, __N, ...)      \
  __N

#define __CA_PLOOC_VA_NUM_ARGS(...)                                               \
  CA_EXPAND(__CA_PLOOC_VA_NUM_ARGS_IMPL(0, __VA_ARGS__, 16, 15, 14, 13, 12, 11, 10, 9, 8, \
                           7, 6, 5, 4, 3, 2, 1, 0))

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
#define CA_CONNECT_IMPL(N, ...) CA_EXPAND(CA_CONNECT##N(__VA_ARGS__))
#define CA_CONNECT_DISPATCH(N, ...) CA_CONNECT_IMPL(N, __VA_ARGS__)
#define CA_CONNECT(...) CA_CONNECT_DISPATCH(CA_VA_NUM_ARGS(__VA_ARGS__), __VA_ARGS__)

/**
 * @brief 选择宏, 根据参数个数N调用对应的__FUNC_N
 */
#define CA_EVAL_IMPL(Func, N, ...) CA_EXPAND(Func##N(__VA_ARGS__))
#define CA_EVAL_DISPATCH(Func, N, ...) CA_EVAL_IMPL(Func, N, __VA_ARGS__)
#define CA_EVAL(Func, ...) CA_EVAL_DISPATCH(Func, CA_VA_NUM_ARGS(__VA_ARGS__), __VA_ARGS__)

/**
 * @brief 给用户用的一些工具 container_of、offsetof等
 * 
 */
#ifndef offsetof
#include <stddef.h>
#endif

#ifndef container_of
// 根据成员指针反推出结构体指针
// GNU/Clang 下使用 typeof 做额外类型约束；其他编译器使用标准 C 版本。
#if defined(__GNUC__) || defined(__clang__)
#define container_of(ptr, type, member) ({                                  \
  const typeof(((type *)0)->member) *__mptr = (ptr);                      \
  (type *)((char *)__mptr - offsetof(type, member));                      \
})
#else
#define container_of(ptr, type, member) \
  ((type *)((char *)(ptr) - offsetof(type, member)))
#endif
#endif


// clang-format on

#endif // !LIBCA_EM_BASE_MACRO_UTIL_H
