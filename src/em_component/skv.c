/**
 * @file skv.c
 * @author canrad (1517807724@qq.com)
 * @brief Simple Key Value format implementation
 * @version 0.1
 * @date 2025-07-28
 * 
 * @copyright Copyright (c) 2025
 * 
 */

#include "skv.h"
#include "../base/string_util.h"

void skv_init(skv_t* skv, skv_kv_t* kvs, u32 num) {
    skv->kvs = kvs;
    skv->num = num;
}

bool skv_put(skv_t* skv, const char* key, const char* value) {
    // 查找是否已存在该key
    for (u32 i = 0; i < skv->num; i++) {
        if (str_cmp(skv->kvs[i].key, key, SKV_MAX_KEY_LEN) == 0) {
            // 找到已存在的key，更新value
            str_cpy(skv->kvs[i].value, value, SKV_MAX_VALUE_LEN);
            return true;
        }
    }
    
    // 查找空槽位
    for (u32 i = 0; i < skv->num; i++) {
        if (skv->kvs[i].key[0] == '\0') {
            // 找到空槽位，插入新键值对
            str_cpy(skv->kvs[i].key, key, SKV_MAX_KEY_LEN);
            str_cpy(skv->kvs[i].value, value, SKV_MAX_VALUE_LEN);
            return true;
        }
    }
    
    // 没有空槽位
    return false;
}

bool skv_get(skv_t* skv, const char* key, char* value) {
    for (u32 i = 0; i < skv->num; i++) {
        if (str_cmp(skv->kvs[i].key, key, SKV_MAX_KEY_LEN) == 0) {
            str_cpy(value, skv->kvs[i].value, SKV_MAX_VALUE_LEN);
            return true;
        }
    }
    
    // 未找到key
    return false;
}

void skv_to_str(skv_t* skv, char* buf) {
    char* p = buf;
    
    for (u32 i = 0; i < skv->num; i++) {
        if (skv->kvs[i].key[0] != '\0') {
            // 格式: key=value\n
            char* key = skv->kvs[i].key;
            char* value = skv->kvs[i].value;
            
            // 复制key
            while (*key) {
                *p++ = *key++;
            }
            
            // 添加等号
            *p++ = '=';
            
            // 复制value
            while (*value) {
                *p++ = *value++;
            }
            
            // 添加换行符
            *p++ = '\n';
        }
    }
    
    // 字符串结束符
    *p = '\0';
}

void skv_from_str(skv_t* skv, char* buf) {
    char* line = str_tok(buf, "\n");
    
    while (line != NULL) {
        // 跳过空行和注释行
        if (line[0] != '\0' && line[0] != '#' && line[0] != ';') {
            // 查找等号
            char* equal_pos = str_chr(line, '=');
            if (equal_pos != NULL && equal_pos != line) {
                // 分割key和value
                *equal_pos = '\0';
                char* key = line;
                char* value = equal_pos + 1;
                
                // 插入键值对
                skv_put(skv, key, value);
            }
        }
        
        line = str_tok(NULL, "\n");
    }
}