/**
 * @file skv.h
 * @author canrad (1517807724@qq.com)
 * @brief 无文件系统的eeprom下的键值存储
 *  
 * @version 0.1
 * @date 2026-01-11
 * 
 * @copyright Copyright (c) 2026
 * 
 */
#ifndef SKV_H
#define SKV_H

#include "../em_base/datatype.h" 

typedef struct skv_port{
    bool (*read)(u32 addr, u8* buf, u32 len);
    bool (*write)(u32 addr, u8* buf, u32 len);
}skv_port_t;

void skv_bind_port(skv_port_t* port);
bool skv_port_is_registered();

/*
  存储设计：分为header区和kv区
  header区(32字节)：
  4bytes magic number + 4bytes number + 4bytes 总的eeprom大小 + 4bytes kv的数量 + 4bytes 下一个kv写入位置 + 12bytes 保留
  kv区：
  1byte item magic number(0x66) + 1byte key长度 + n bytes key + 1byte value类型 + 1byte value长度 + n bytes value + 2byte crc16
 
  key最大是255字节的字符串
  value最大是255字节的二进制数据
  */
#define SKV_MAX_KEY_LEN 255
#define SKV_MAX_VALUE_LEN 255

#define SKV_TYPE_U8 0x01
#define SKV_TYPE_U16 0x02
#define SKV_TYPE_U32 0x03
#define SKV_TYPE_U64 0x04
#define SKV_TYPE_I8 0x05
#define SKV_TYPE_I16 0x06
#define SKV_TYPE_I32 0x07
#define SKV_TYPE_F32 0x08
#define SKV_TYPE_F64 0x09
#define SKV_TYPE_STRING 0x0A
#define SKV_TYPE_BLOB 0x0B

typedef struct skv_kv_item {
    u8 key_length;
    char* key;
    u8 value_type;
    u8 value_length;
    void* value;
}skv_kv_item_t;

#define SKV_MAGIC_NUMBER 0x534B5631 // "SKV1"
#define SKV_ITEM_MAGIC 0x66

#define SKV_ERR_NOT_FOUND -1

typedef struct skv {
    u32 num;
    u32 next_addr;
    u32 total_size;
    u32 start_addr;
}skv_t;

// 初始化skv结构体
void skv_init(skv_t* skv, u32 start_addr);
// 读取header
// 如果返回false，表示header无效
bool skv_read_header(skv_t* skv);
// 写入header
bool skv_write_header(skv_t* skv);
// 写入键值对
i32 skv_put_item(skv_t* skv, skv_kv_item_t* item);
// 读取键值对
i32 skv_get_item(skv_t* skv, skv_kv_item_t* item);



#endif // !SKV_H
