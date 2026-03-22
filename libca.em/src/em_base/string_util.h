/**
 * @file string_util.h
 * @author canrad (1517807724@qq.com)
 * @brief 字符串工具函数的实现
 * @version 0.2
 * @date 2025-07-28
 * @update 2026-01-31 明确已有的字符串操作函数的行为和语义
 * @update 2026-03-05 增加 USE_CUSTOM_STRING_UTIL_IMPL 开关，支持标准库内联实现
 *
 * @copyright Copyright (c) 2025
 *
 */
#ifndef LIBCA_EM_BASE_STRING_UTIL_H
#define LIBCA_EM_BASE_STRING_UTIL_H

#include "datatype.h"

/* 使用自定义实现开关：
 * - 未定义或定义为 0：使用标准库内联实现（推荐，性能更优）
 * - 定义为 1：使用自定义实现（适用于无标准库环境的嵌入式场景）
 *
 * 注意：仅部分函数支持标准库内联实现，以下函数因语义差异始终使用自定义实现：
 * - str_cpy, str_cat：返回值语义不同（返回长度而非指针）
 * - str_trim, str_ltrim, str_rtrim：标准库无对应函数
 * - str_to_upper, str_to_lower, str_reverse：标准库无对应函数
 * - str_starts_with, str_ends_with 系列：标准库无对应函数
 */
#ifndef USE_CUSTOM_STRING_UTIL_IMPL
#define USE_CUSTOM_STRING_UTIL_IMPL 0
#endif

#if !USE_CUSTOM_STRING_UTIL_IMPL
#include <string.h>
#endif

#ifdef __cplusplus
extern "C" {
#endif

#define STR_OK              (0)
// 空指针错误
#define STR_ERR_NULL        (-1)
// 缓冲区大小不足
#define STR_ERR_SIZE        (-2)
// 非法大小
#define STR_ERR_INVALID     (-3)

/* ==================== 字符操作函数（始终使用自定义实现） ==================== */

/**
 * @brief 字符转小写
 *
 * @param c 字符
 * @return char 转小写的字符
 */
char char_to_lower(char c);

/**
 * @brief 字符转大写
 *
 * @param c 字符
 * @return char 转大写的字符
 */
char char_to_upper(char c);

/* ==================== 可标准库优化的函数 ==================== */

#if USE_CUSTOM_STRING_UTIL_IMPL
/* ==================== 自定义实现版本 ==================== */

/**
 * @brief 获取字符串长度
 *
 * @param str 字符串指针
 * @return usize 字符串长度，若 str 为 NULL 返回 0
 */
usize str_len(const char* str);

/**
 * @brief 获取字符串长度（带最大长度限制，更安全）
 *
 * @param str 字符串指针
 * @param max_len 最大检查长度
 * @return usize 字符串长度，若 str 为 NULL 返回 0
 */
usize str_nlen(const char* str, usize max_len);

/**
 * @brief 比较两个字符串
 *
 * @param s1 字符串1
 * @param s2 字符串2
 * @param size 最大比较长度
 * @return i32 相等返回 0，不等返回差值。
 *         若 s1 为 NULL 且 s2 不为 NULL 返回 -1，反之返回 1。
 *         若两者都为 NULL 或 size 为 0 返回 0。
 */
i32 str_cmp(const char* s1, const char* s2, usize size);

/**
 * @brief 在字符串中查找字符
 *
 * @param str 字符串
 * @param c 要查找的字符
 * @return char* 指向查找到的字符的指针，未找到或 str 为 NULL 返回 NULL
 */
char* str_find_ch(const char* str, char c);

/**
 * @brief 在字符串中查找子串
 *
 * @param haystack 主字符串
 * @param needle 要查找的子串
 * @return char* 指向查找到的子串的指针，未找到或参数为 NULL 返回 NULL
 */
char* str_find_str(const char* haystack, const char* needle);

#else
/* ==================== 标准库内联实现版本 ==================== */

/**
 * @brief 获取字符串长度
 *
 * @param str 字符串指针
 * @return usize 字符串长度，若 str 为 NULL 返回 0
 */
static inline usize str_len(const char* str)
{
    if (!str) {
        return 0;
    }
    return strlen(str);
}

/**
 * @brief 获取字符串长度（带最大长度限制，更安全）
 *
 * @param str 字符串指针
 * @param max_len 最大检查长度
 * @return usize 字符串长度，若 str 为 NULL 返回 0
 */
static inline usize str_nlen(const char* str, usize max_len)
{
    if (!str) {
        return 0;
    }
    return strnlen(str, max_len);
}

/**
 * @brief 比较两个字符串
 *
 * @param s1 字符串1
 * @param s2 字符串2
 * @param size 最大比较长度
 * @return i32 相等返回 0，不等返回差值。
 *         若 s1 为 NULL 且 s2 不为 NULL 返回 -1，反之返回 1。
 *         若两者都为 NULL 或 size 为 0 返回 0。
 */
static inline i32 str_cmp(const char* s1, const char* s2, usize size)
{
    if (s1 == s2 || size == 0) {
        return 0;
    }
    if (!s1) {
        return -1;
    }
    if (!s2) {
        return 1;
    }
    return (i32)strncmp(s1, s2, size);
}

/**
 * @brief 在字符串中查找字符
 *
 * @param str 字符串
 * @param c 要查找的字符
 * @return char* 指向查找到的字符的指针，未找到或 str 为 NULL 返回 NULL
 */
static inline char* str_find_ch(const char* str, char c)
{
    if (!str) {
        return NULL;
    }
    return strchr(str, (int)c);
}

/**
 * @brief 在字符串中查找子串
 *
 * @param haystack 主字符串
 * @param needle 要查找的子串
 * @return char* 指向查找到的子串的指针，未找到或参数为 NULL 返回 NULL
 */
static inline char* str_find_str(const char* haystack, const char* needle)
{
    if (!haystack || !needle) {
        return NULL;
    }
    return strstr(haystack, needle);
}

#endif /* USE_CUSTOM_STRING_UTIL_IMPL */

/* ==================== 始终使用自定义实现的函数 ==================== */

/**
 * @brief 字符串复制（确保以 \0 结尾）
 *
 * @param dest 目标缓冲区
 * @param src 源字符串
 * @param size 目标缓冲区总大小
 * @return i32 成功返回实际复制的长度，失败返回错误码
 */
i32 str_cpy(char* dest, const char* src, usize size);

/**
 * @brief 字符串拼接（确保以 \0 结尾）
 *
 * @param dest 目标缓冲区
 * @param src 源字符串
 * @param dest_max_size 目标缓冲区总大小
 * @return i32 成功返回实际拼接后的长度，失败返回错误码
 */
i32 str_cat(char* dest, const char* src, usize dest_max_size);

/**
 * @brief 判断两个字符串是否完全相等
 *
 * @param s1 字符串1
 * @param s2 字符串2
 * @return bool 相等返回 true，否则返回 false
 */
bool str_is_equal(const char* s1, const char* s2);

/**
 * @brief 去除字符串首尾的空白字符（原地实现）
 * 
 * @param str 字符串
 * @return i32 去除后的字符串长度
 */
i32 str_trim(char* str);

/**
 * @brief 去除字符串左侧空白字符（原地实现）
 *
 * @param str 字符串
 * @return i32 去除后的字符串长度
 */
i32 str_ltrim(char* str);

/**
 * @brief 去除字符串右侧空白字符（原地实现）
 *
 * @param str 字符串
 * @return i32 去除后的字符串长度
 */
i32 str_rtrim(char* str);

/**
 * @brief 字符串转大写
 * 
 * @param str 字符串
 */
void str_to_upper(char* str);

/**
 * @brief 字符串转小写
 * 
 * @param str 字符串
 */
void str_to_lower(char* str);

/**
 * @brief 翻转字符串
 * 
 * @param str 字符串
 */
void str_reverse(char* str);

/**
 * @brief 判断字符串是否以特定前缀开始
 * 
 * @param str 字符串
 * @param prefix 前缀
 * @return bool 是返回 true
 */
bool str_starts_with(const char* str, const char* prefix);

/**
 * @brief 判断字符串是否以特定前缀开始（忽略大小写）
 * 
 * @param str 字符串
 * @param prefix 前缀
 * @return bool 是返回 true
 */
bool str_starts_with_i(const char* str, const char* prefix);

/**
 * @brief 判断字符串是否以特定后缀结尾
 * 
 * @param str 字符串
 * @param suffix 后缀
 * @return bool 是返回 true
 */
bool str_ends_with(const char* str, const char* suffix);

/**
 * @brief 判断字符串是否以特定后缀结尾（忽略大小写）
 * 
 * @param str 字符串
 * @param suffix 后缀
 * @return bool 是返回 true
 */
bool str_ends_with_i(const char* str, const char* suffix);

#ifdef __cplusplus
}
#endif

#endif   // !LIBCA_EM_BASE_STRING_UTIL_H
