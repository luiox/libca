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

///////////////////////////////////////////////////////////////////
// 字符串拷贝
char* str_copy(char* dest, const char* src);
// 字符串拼接
char* str_concat(char* dest, const char* src);
// 字符串长度
u32 str_length(const char* str);
// 字符串比较
s32 str_compare(const char* str1, const char* str2);
// 字符串搜索字串
char* str_find(const char* str, const char* substr);
// 字符串替换
char* str_replace(char* str, const char* old, const char* new);
// 去除字符串首尾的空格、\t、\n等空白字符
char* str_trim(char* str);
///////////////////////////////////////////////////////////////////////////////
// 内存设置
void* mem_set(void* ptr, uint8_t value, uint32_t num);
// 内存复制
void* mem_copy(void* dest, const void* src, uint32_t num);
// 内存移动
void* mem_move(void* dest, const void* src, uint32_t num);
// 内存比较
int32_t mem_compare(const void* ptr1, const void* ptr2, uint32_t num);
// 十六进制转换
char* to_hex(const void* data, uint32_t data_len, char* buf, uint32_t buf_len);
// 整数到字符串转换，最多16进制
char* int_to_str(int32_t value, char* str, int8_t base);
// 字符串到整数转换
int32_t str_to_int(const char* str, char** endptr, int8_t base);
// 反转字符串
void reverse_str(char* str);
// 复制字符串到新分配的内存
char* str_duplicate(const char* str);
// 获取子字符串
char* str_substr(const char* str, uint32_t start, uint32_t len);
// 字符串分词
int str_tokenize(char* str, const char* delimiters, char** tokens, int max_tokens);


#endif   // !STRING_UTIL_H