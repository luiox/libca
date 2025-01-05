#ifndef STRING_H
#define STRING_H

#include <libca/core/stdtype.h>

// 字符串拷贝
char* str_copy(char* dest, const char* src);
// 字符串拼接
char* str_concat(char* dest, const char* src);
// 字符串长度
uint32_t str_length(const char* str);
// 字符串比较
int32_t str_compare(const char* str1, const char* str2);
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

#endif   // !STRING_H
