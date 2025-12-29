/**
 * @file skv.h
 * @author canrad (1517807724@qq.com)
 * @brief Simple Key Value format
 *  目的是简化无文件系统，无操作系统下的配置存储
 *  面向flash这种情况下的程序配置管理
 * @version 0.1
 * @date 2025-07-28
 * 
 * @copyright Copyright (c) 2025
 * 
 */
#ifndef SKV_H
#define SKV_H

#include "../em_base/datatype.h" 
#include <stdbool.h>

#define SKV_MAX_KEY_LEN 16
#define SKV_MAX_VALUE_LEN 16

typedef struct{
    char key[SKV_MAX_KEY_LEN];
    char value[SKV_MAX_VALUE_LEN];
}skv_kv_t;

typedef struct {
    skv_kv_t* kvs;
    u32 num;
}skv_t;

void skv_init(skv_t* skv, skv_kv_t* kvs, u32 num);
bool skv_put(skv_t* skv, const char* key, const char* value);
bool skv_get(skv_t* skv, const char* key, char* value);
void skv_to_str(skv_t* skv, char* buf);
void skv_from_str(skv_t* skv, char* buf);


#endif // !SKV_H
