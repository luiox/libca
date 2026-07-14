///
/// @file ini.c
/// @author canrad (1517807724@qq.com)
/// @brief 纯内存式的ini文件读写实现，适用于嵌入式环境
/// @version 0.1
/// @date 2025-07-28
///
/// @copyright Copyright (c) 2025
///

#include "ini.h"
#include <em_base/string_util.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>




// // 将u32转换为小端序字节数组
// static void u32_to_le_bytes(u32 value, u8* bytes) {
//     bytes[0] = (u8)(value & 0xFF);
//     bytes[1] = (u8)((value >> 8) & 0xFF);
//     bytes[2] = (u8)((value >> 16) & 0xFF);
//     bytes[3] = (u8)((value >> 24) & 0xFF);
// }

// // 从小端序字节数组转换为u32
// static u32 le_bytes_to_u32(u8* bytes) {
//     return (u32)bytes[0] | 
//            ((u32)bytes[1] << 8) | 
//            ((u32)bytes[2] << 16) | 
//            ((u32)bytes[3] << 24);
// }

// // 去除字符串前后空格
// static void trim_string(char* str, char* result, u32 max_len) {
//     // 找到字符串开始位置（跳过前导空格）
//     while (*str == ' ' || *str == '\t') {
//         str++;
//     }
    
//     // 计算有效长度
//     u32 len = 0;
//     char* temp = str;
//     while (*temp && *temp != '\n' && *temp != '\r') {
//         len++;
//         temp++;
//     }
    
//     // 去掉尾部空格
//     while (len > 0 && (str[len-1] == ' ' || str[len-1] == '\t')) {
//         len--;
//     }
    
//     // 复制结果
//     u32 copy_len = (len < max_len-1) ? len : max_len-1;
//     u32 i;
//     for (i = 0; i < copy_len; i++) {
//         result[i] = str[i];
//     }
//     result[i] = '\0';
// }

// // 解析INI字符串
// void ini_parse(u8* buf, ini_section_t** sections, u32* section_num) {
//     // 计算行数
//     u32 line_count = 0;
//     u8* temp_buf = buf;
//     while (*temp_buf != '\0') {
//         if (*temp_buf == '\n') {
//             line_count++;
//         }
//         temp_buf++;
//     }
//     // 如果最后一行没有换行符，也需要计数
//     if (temp_buf > buf && *(temp_buf-1) != '\n' && *(temp_buf-1) != '\0') {
//         line_count++;
//     }
    
//     // 如果没有行，直接返回
//     if (line_count == 0) {
//         *sections = NULL;
//         *section_num = 0;
//         return;
//     }
    
//     // 分配内存
//     *sections = (ini_section_t*)calloc(line_count, sizeof(ini_section_t));
//     if (*sections == NULL) {
//         *section_num = 0;
//         return;
//     }
    
//     u32 count = 0;
//     char* line = str_tok((char*)buf, "\n");
    
//     while (line != NULL && count < line_count) {
//         // 跳过空行和注释行
//         if (line[0] == '\0' || line[0] == ';' || line[0] == '#') {
//             line = str_tok(NULL, "\n");
//             continue;
//         }
        
//         // 查找等号
//         char* equal_pos = str_chr(line, '=');
//         if (equal_pos != NULL && equal_pos != line) {
//             // 分割键和值
//             *equal_pos = '\0';
//             char* key = line;
//             char* value = equal_pos + 1;
            
//             // 去除键和值前后的空格
//             char key_trimmed[INI_MAX_KEY_LEN];
//             char value_trimmed[INI_MAX_VALUE_LEN];
            
//             trim_string(key, key_trimmed, INI_MAX_KEY_LEN);
//             trim_string(value, value_trimmed, INI_MAX_VALUE_LEN);
            
//             // 检查长度限制
//             if (str_len(key_trimmed) < INI_MAX_KEY_LEN-1 && str_len(value_trimmed) < INI_MAX_VALUE_LEN-1) {
//                 str_cpy((*sections)[count].key, key_trimmed, INI_MAX_KEY_LEN);
//                 str_cpy((*sections)[count].value, value_trimmed, INI_MAX_VALUE_LEN);
//                 count++;
//             }
//         }
        
//         line = str_tok(NULL, "\n");
//     }
    
//     *section_num = count;
// }

// // 获取指定键的值
// u8* ini_get(ini_section_t* sections, u32 section_num, char* key) {
//     for (u32 i = 0; i < section_num; i++) {
//         if (str_cmp(sections[i].key, key, str_len(key)) == 0 && 
//             str_len(sections[i].key) == str_len(key)) {
//             return (u8*)sections[i].value;
//         }
//     }
//     return NULL;
// }

// // 设置指定键的值
// void ini_set(ini_section_t* sections, u32 section_num, char* key, char* value) {
//     // 首先查找是否已存在该键
//     for (u32 i = 0; i < section_num; i++) {
//         if (str_cmp(sections[i].key, key, str_len(key)) == 0 && 
//             str_len(sections[i].key) == str_len(key)) {
//             // 找到则更新值
//             str_cpy(sections[i].value, value, INI_MAX_VALUE_LEN);
//             return;
//         }
//     }
    
//     // 未找到则添加新的键值对（假设有足够空间）
//     str_cpy(sections[section_num].key, key, INI_MAX_KEY_LEN);
//     str_cpy(sections[section_num].value, value, INI_MAX_VALUE_LEN);
// }

// // 将INI配置转换为字符串
// void ini_to_str(ini_section_t* sections, u32 section_num, u8* buf) {
//     u8* ptr = buf;
//     u32 remaining_size = 1024; // 假设缓冲区足够大
    
//     for (u32 i = 0; i < section_num; i++) {
//         // 计算需要的长度（key=value\n）
//         u32 key_len = str_len(sections[i].key);
//         u32 value_len = str_len(sections[i].value);
//         u32 needed_len = key_len + 1 + value_len + 1; // +1 for = and +1 for \n
        
//         // 检查是否有足够空间
//         if (remaining_size > needed_len) {
//             // 写入 key=
//             u32 j;
//             for (j = 0; j < key_len && remaining_size > 1; j++) {
//                 *ptr++ = sections[i].key[j];
//                 remaining_size--;
//             }
            
//             if (remaining_size > 1) {
//                 *ptr++ = '=';
//                 remaining_size--;
//             }
            
//             // 写入 value
//             for (j = 0; j < value_len && remaining_size > 1; j++) {
//                 *ptr++ = sections[i].value[j];
//                 remaining_size--;
//             }
            
//             // 写入换行符
//             if (remaining_size > 1) {
//                 *ptr++ = '\n';
//                 remaining_size--;
//             }
//         }
//     }
    
//     // 确保字符串结束
//     if (remaining_size > 0) {
//         *ptr = '\0';
//     }
// }

void f()
{
    ini_kv_t lpid_kvs[3];
    ini_kv_t rpid_kvs[3];
    ini_section_t lpid_section = {"lpid", lpid_kvs};
    ini_section_t rpid_section = {"rpid", rpid_kvs};
    ini_section_t sections[] = {lpid_section, rpid_section};
    ini_t ini = {sections, array_size(sections)};
    u8 buf[1024];
    // ini_to_str(&ini, buf);

}


