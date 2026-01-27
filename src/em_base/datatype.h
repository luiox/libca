/**
 * @file datatype.h
 * @author canrad (1517807724@qq.com)
 * @brief 基础类型的定义
 * 位，字节，字节序相关的操作
 * @version 0.2
 * @date 2025-07-21
 *
 * @copyright Copyright (c) 2025
 *
 */
#ifndef DATATYPE_H
#define DATATYPE_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

// 整数
typedef uint8_t      u8;
typedef uint16_t     u16;
typedef uint32_t     u32;
typedef int8_t       i8;
typedef int16_t      i16;
typedef int32_t      i32;
#if USE_RUST_STYLE_INT
typedef s8   i8;
typedef s16  i16;
typedef s32  i32;
#endif
#ifdef HAS_INT64
typedef uint64_t     u64;
typedef int64_t      i64;
#    if USE_RUST_STYLE_INT
typedef i64  i64;
#    endif
#endif
// 浮点数
typedef float        f32;
typedef double       f64;
// size
typedef size_t          usize;

// 常用函数指针
typedef void(*runnable_fn_t)(void);
// 暂时不开放谓词
//typedef bool(*predicate_fn_t)(void* arg);

// 获取数组元素个数
#define array_size(arr) (sizeof(arr) / sizeof((arr)[0]))

// 判断一个变量是否为无符号类型
#define is_unsigned_v(a) (a >= 0 && ~a >= 0)

// 判断一个类型是否为无符号类型
#define is_unsigned_t(type) ((type)0 - 1 > 0)

// 标记未使用的参数
// 例: unused_param(a);
#define unused_param(param) (void)(param)

// 时间戳类型，单位微秒，开机后，一般来说基于SysTick或其他时基
#if HAS_INT64
typedef u64 timestamp_t;
#else
// 如果没有64位，使用32位时间戳
typedef u32 timestamp_t;
#endif

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

#endif   // !DATATYPE_H
