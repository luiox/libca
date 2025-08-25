/**
 * @file test-ini.c
 * @author canrad (1517807724@qq.com)
 * @brief ini模块的测试文件，模拟flash读写操作
 * @version 0.1
 * @date 2025-07-28
 * 
 * @copyright Copyright (c) 2025
 * 
 */

#include "component/ini.h"
#include "base/string_util.h"
#include <stdio.h>
#include <stdlib.h>
#include <locale.h>

// // 模拟flash存储的文件名
// #define FLASH_FILE "flash_emulation.bin"

// // 全局缓冲区，用于模拟flash存储
// static u8 flash_buffer[1024];
// static u32 flash_size = 0;

// // 模拟从flash读取配置字符串大小
// u32 ini_read_str_size() {
//     FILE* fp = fopen(FLASH_FILE, "rb");
//     if (fp == NULL) {
//         return 0;
//     }
    
//     u32 size;
//     fread(&size, sizeof(u32), 1, fp);
//     fclose(fp);
    
//     return size;
// }

// // 模拟从flash读取配置字符串
// void ini_read_str(u8* buf) {
//     FILE* fp = fopen(FLASH_FILE, "rb");
//     if (fp == NULL) {
//         buf[0] = '\0';
//         return;
//     }
    
//     u32 size;
//     fread(&size, sizeof(u32), 1, fp);
//     fread(buf, sizeof(u8), size, fp);
//     buf[size] = '\0';
    
//     fclose(fp);
// }

// // 模拟向flash写入配置字符串大小
// void ini_write_str_size(u32 size) {
//     FILE* fp = fopen(FLASH_FILE, "wb");
//     if (fp == NULL) {
//         return;
//     }
    
//     fwrite(&size, sizeof(u32), 1, fp);
//     fclose(fp);
// }

// // 模拟向flash写入配置字符串
// void ini_write_str(u8* buf) {
//     FILE* fp = fopen(FLASH_FILE, "ab");
//     if (fp == NULL) {
//         return;
//     }
    
//     u32 size = str_len((char*)buf) + 1;
//     fwrite(buf, sizeof(u8), size, fp);
    
//     fclose(fp);
// }

// // 测试函数
// void test_ini() {
//     printf("开始测试INI模块\n");
    
//     // 创建测试数据
//     ini_section_t sections[10];
//     u32 section_num = 3;
    
//     // 初始化一些测试数据
//     str_cpy(sections[0].key, "device_id", INI_MAX_KEY_LEN);
//     str_cpy(sections[0].value, "12345", INI_MAX_VALUE_LEN);
    
//     str_cpy(sections[1].key, "baud_rate", INI_MAX_KEY_LEN);
//     str_cpy(sections[1].value, "115200", INI_MAX_VALUE_LEN);
    
//     str_cpy(sections[2].key, "timeout", INI_MAX_KEY_LEN);
//     str_cpy(sections[2].value, "5000", INI_MAX_VALUE_LEN);
    
//     // 将配置写入flash
//     u8 buf[512];
//     ini_to_str(sections, section_num, buf);
//     printf("写入的配置内容:\n%s\n", buf);
    
//     u32 size = strlen((char*)buf) + 1;
//     ini_write_str_size(size);
//     ini_write_str(buf);
    
//     // 从flash读取配置
//     u32 read_size = ini_read_str_size();
//     printf("从flash读取到的大小: %d\n", read_size);
    
//     u8* read_buf = (u8*)malloc(read_size);
//     ini_read_str(read_buf);
//     printf("从flash读取到的内容:\n%s\n", read_buf);
    
//     // 解析读取到的内容
//     ini_section_t* parsed_sections;
//     u32 parsed_section_num;
//     ini_parse(read_buf, &parsed_sections, &parsed_section_num);
    
//     printf("解析出%d个配置项:\n", parsed_section_num);
//     for (u32 i = 0; i < parsed_section_num; i++) {
//         printf("  %s = %s\n", parsed_sections[i].key, parsed_sections[i].value);
//     }
    
//     // 测试获取特定键值
//     u8* value = ini_get(parsed_sections, parsed_section_num, "device_id");
//     if (value != NULL) {
//         printf("device_id的值为: %s\n", value);
//     } else {
//         printf("未找到device_id\n");
//     }
    
//     // 测试设置键值
//     ini_set(parsed_sections, parsed_section_num, "new_key", "new_value");
//     value = ini_get(parsed_sections, parsed_section_num+1, "new_key");
//     if (value != NULL) {
//         printf("new_key的值为: %s\n", value);
//     }
    
//     // 清理内存
//     free(parsed_sections);
//     free(read_buf);
    
//     printf("测试完成\n");
// }

int main() {
    setlocale(LC_ALL, "zh_CN.UTF-8");
    // test_ini();
    return 0;
}