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
#include <stdbool.h>

// 字符串长度（类似 strlen，但更安全）
usize str_len(const char* str);

usize str_nlen(const char* str, usize max_len);

// 字符串复制（类似 strncpy，但确保终止符），大于0时，返回实际复制的字符串长度，小于0时，返回的是错误码
i32 str_cpy(char* dest, const char* src, usize size);

// 字符串拼接，大于0时，返回实际复制的字符串长度，小于0时，返回的是错误码
i32 str_cat(char* dest, const char* src, usize dest_max_size);

// 字符串比较（类似 strncmp）
i32 str_cmp(const char* s1, const char* s2, usize size);

// 对str_cmp的一个封装，方便if (str_is_equal(s1, "START")) 这种判断
bool str_is_equal(const char* s1, const char* s2);

// 查找字符（类似 strchr）
char* str_find_ch(const char* str, char c);

// 查找子串（类似 strstr）
char* str_find_str(const char* haystack, const char* needle);

// 去除字符串首尾的空格、\t、\n等空白字符，原地实现，返回去除以后字符串的长度
i32 str_trim(char* str);

// 字符串转大写
void str_to_upper(char* str);

// 字符串转小写
void str_to_lower(char* str);

// 反转字符串
void str_reverse(char* str);

// 判断是否以某个字符串开始
bool str_starts_with(const char* str, const char* prefix);

// 判断是否以某个字符串开始（忽略大小写）
bool str_starts_with_i(const char* str, const char* prefix);

// 判断是否以某个字符串结尾
bool str_ends_with(const char* str, const char* suffix);

// 判断是否以某个字符串结尾（忽略大小写）
bool str_end_with_i(const char* str, const char* suffix);

///////////////////////////////////////////////////////////////////////////////
// 内存设置
void* mem_set(void* ptr, uint8_t value, uint32_t num);
// 内存复制
void* mem_copy(void* dest, const void* src, uint32_t num);
// 内存移动
void* mem_move(void* dest, const void* src, uint32_t num);
// 内存比较
int32_t mem_compare(const void* ptr1, const void* ptr2, uint32_t num);

///////////////////////////////////////////////////////////////////////////////
// 十六进制转换
char* to_hex(const void* data, uint32_t data_len, char* buf, uint32_t buf_len);

// 十六进制字符串转整数
bool hex_str_to_uint(const char* str, u32* out_value);

// 整数转十六进制字符串
void uint_to_hex_str(u32 value, char* out_str, usize out_size);

#endif   // !LIBCA_EM_BASE_STRING_UTIL_H
