/**
 * @file ini.h
 * @author canrad (1517807724@qq.com)
 * @brief 纯内存式的ini文件读写，适用于嵌入式环境
 * @version 0.1
 * @date 2025-07-28
 * 
 * @copyright Copyright (c) 2025
 * 
 */
#ifndef INI_H
#define INI_H 

#include "../base/datatype.h" 

// 需要移植的部分
// 读取配置字符串大小
u32 ini_read_str_size();
// 读取配置字符串
void ini_read_str(u8* buf);
// 写入配置字符串大小
void ini_write_str_size(u32 size);
// 写入配置字符串
void ini_write_str(u8* buf);

//
// 设计思路
// 用户定义一个ini_kv_t数组存储一个section的kv，
// 因为用户可以确定自己的每个section有多少kv
// 其次，用户需要定义ini_section_t表示节
// 最后需要一个ini_t来管理这些section_t
//
#define INI_MAX_KEY_LEN 16
#define INI_MAX_VALUE_LEN 16
#define INI_MAX_SECTION_NAME_LEN 16

typedef struct{
    char key[INI_MAX_KEY_LEN];
    char value[INI_MAX_VALUE_LEN];
}ini_kv_t;

typedef struct{
    char name[INI_MAX_SECTION_NAME_LEN];
    ini_kv_t* kvs;
}ini_section_t;

typedef struct{
    ini_section_t* sections;
    u32 section_num;
}ini_t;

// 转字符串
void ini_to_str(ini_t* ini, u8* buf);

// // 解析
// void ini_parse(u8* buf, ini_section_t** sections, u32* section_num);
// // 获取
// u8* ini_get(ini_section_t* sections, u32 section_num, char* key);
// // 设置
// void ini_set(ini_section_t* sections, u32 section_num, char* key, char* value);




#endif // !INI_H
