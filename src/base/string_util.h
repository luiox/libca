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
#ifndef STRING_UTIL_H
#define STRING_UTIL_H

#include "datatype.h"

// 字符串长度（类似 strlen，但更安全）
usize str_len(const char* str);

// 字符串复制（类似 strncpy，但确保终止符）
char* str_cpy(char* dest, const char* src, usize size);

// 字符串比较（类似 strncmp）
int str_cmp(const char* s1, const char* s2, usize size);

// 查找字符（类似 strchr）
char* str_chr(const char* str, char c);

// 查找子串（类似 strstr）
char* str_str(const char* haystack, const char* needle);

// 字符串分割（类似 strtok）
char* str_tok(char* str, const char* delim);

// 字符串转大写
void str_to_upper(char* str);

// 字符串转小写
void str_to_lower(char* str);

// 反转字符串
void str_reverse(char* str);

// 十六进制字符串转整数
bool hex_str_to_uint(const char* str, u32* out_value);

// 整数转十六进制字符串
void uint_to_hex_str(u32 value, char* out_str, usize out_size);


#endif   // !STRING_UTIL_H