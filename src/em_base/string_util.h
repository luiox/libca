/**
 * @file string_util.h
 * @author canrad (1517807724@qq.com)
 * @brief 字符串工具函数的实现
 * @version 0.2
 * @date 2025-07-28
 * 
 * @copyright Copyright (c) 2025
 * 
 */
#ifndef LIBCA_EM_BASE_STRING_UTIL_H
#define LIBCA_EM_BASE_STRING_UTIL_H

#include "datatype.h"

#ifdef __cplusplus
extern "C" {
#endif

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

/**
 * @brief 获取字符串长度
 * 
 * @param str 字符串指针
 * @return usize 字符串长度
 */
usize str_len(const char* str);

/**
 * @brief 获取字符串长度（带最大长度限制，更安全）
 * 
 * @param str 字符串指针
 * @param max_len 最大检查长度
 * @return usize 字符串长度
 */
usize str_nlen(const char* str, usize max_len);

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
 * @brief 比较两个字符串
 * 
 * @param s1 字符串1
 * @param s2 字符串2
 * @param size 最大比较长度
 * @return i32 相等返回 0，不等返回差值
 */
i32 str_cmp(const char* s1, const char* s2, usize size);

/**
 * @brief 判断两个字符串是否完全相等
 * 
 * @param s1 字符串1
 * @param s2 字符串2
 * @return bool 相等返回 true，否则返回 false
 */
bool str_is_equal(const char* s1, const char* s2);

/**
 * @brief 在字符串中查找字符
 * 
 * @param str 字符串
 * @param c 要查找的字符
 * @return char* 指向查找到的字符的指针，未找到返回 NULL
 */
char* str_find_ch(const char* str, char c);

/**
 * @brief 在字符串中查找子串
 * 
 * @param haystack 主字符串
 * @param needle 要查找的子串
 * @return char* 指向查找到的子串的指针，未找到返回 NULL
 */
char* str_find_str(const char* haystack, const char* needle);

/**
 * @brief 去除字符串首尾的空白字符（原地实现）
 * 
 * @param str 字符串
 * @return i32 去除后的字符串长度
 */
i32 str_trim(char* str);

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


#if 0
///////////////////////////////////////////////////////////////////////////////
// 内存操作
void* mem_set(void* ptr, u8 value, u32 num);
void* mem_copy(void* dest, const void* src, u32 num);
void* mem_move(void* dest, const void* src, u32 num);
i32 mem_compare(const void* ptr1, const void* ptr2, u32 num);

///////////////////////////////////////////////////////////////////////////////
// 十六进制转换
char* to_hex(const void* data, u32 data_len, char* buf, u32 buf_len);

/**
 * @brief 十六进制字符串转整数
 * 
 * @param str 字符串
 * @param out_value 输出值
 * @return bool 成功返回 true
 */
bool hex_str_to_uint(const char* str, u32* out_value);

/**
 * @brief 整数转十六进制字符串
 * 
 * @param value 整数
 * @param out_str 输出缓冲区
 * @param out_size 缓冲区大小
 */
void uint_to_hex_str(u32 value, char* out_str, usize out_size);

#endif

#ifdef __cplusplus
}
#endif

#endif   // !LIBCA_EM_BASE_STRING_UTIL_H
