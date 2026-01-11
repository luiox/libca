#include "skv.h"
#include "../em_util/crc.h"
#include <string.h>

static skv_port_t* g_skv_port = NULL;

void skv_bind_port(skv_port_t* port)
{
    g_skv_port = port;
}

bool skv_port_is_registered()
{
    return g_skv_port != NULL;
}

void skv_init(skv_t* skv, u32 start_addr)
{
    skv->total_size = 0;
    skv->num        = 0;
    skv->next_addr  = start_addr + 32;   // header区32字节
    skv->start_addr = start_addr;

    // 先尝试读取
    bool valid = skv_read_header(skv);
    if (!valid) {
        // 如果header无效，初始化header
        skv_write_header(skv);
    }
}

// 读取header
// 如果返回false，表示header无效
bool skv_read_header(skv_t* skv)
{
    u8 buf[32];
    if (!g_skv_port->read(skv->start_addr, buf, 32)) {
        return false;
    }
    // 解析header
    u32 magic = *((u32*)&buf[0]);
    if (magic != SKV_MAGIC_NUMBER) {
        return false;
    }
    skv->num        = *((u32*)&buf[4]);
    skv->total_size = *((u32*)&buf[8]);
    skv->next_addr  = *((u32*)&buf[12]);
    return true;
}

bool skv_write_header(skv_t* skv)
{
    u8 buf[32] = {0};
    // 填充header
    *((u32*)&buf[0])  = SKV_MAGIC_NUMBER;
    *((u32*)&buf[4])  = skv->num;
    *((u32*)&buf[8])  = skv->total_size;
    *((u32*)&buf[12]) = skv->next_addr;
    return g_skv_port->write(skv->start_addr, buf, 32);
}

// 写入键值对
i32 skv_put_item(skv_t* skv, skv_kv_item_t* item)
{
    u32 next_addr = skv->next_addr;
    // 计算需要的空间
    // 1byte item magic number(0x66) + 1byte key长度 + n bytes key + 1byte value类型 + 1byte
    // value长度 + n bytes value + 2byte crc16
    u32 needed_size = 1 + 1 + item->key_length + 1 + 1 + item->value_length + 2;
    if (next_addr + needed_size > skv->start_addr + skv->total_size) {
        // 空间不足
        return -1;
    }
    // 计算crc16-modebus
    u8 item_magic = SKV_ITEM_MAGIC;

    u16 crc = CRC16_MODEBUS_START_VALUE;
    // 计算item magic number
    crc = crc16_modbus_ex(&item_magic, 1, crc);
    // 计算key长度和key
    crc = crc16_modbus_ex(&item->key_length, 1, crc);
    crc = crc16_modbus_ex((u8*)item->key, item->key_length, crc);
    // 计算value类型和value长度
    crc = crc16_modbus_ex(&item->value_type, 1, crc);
    crc = crc16_modbus_ex(&item->value_length, 1, crc);
    // 计算value
    crc = crc16_modbus_ex((u8*)item->value, item->value_length, crc);

    // 写入数据
    bool ret = true;
    // 写入item magic number
    ret &= g_skv_port->write(next_addr, &item_magic, 1);
    next_addr += 1;
    // 写入key
    ret &= g_skv_port->write(next_addr, &item->key_length, 1);
    next_addr += 1;
    ret &= g_skv_port->write(next_addr, (u8*)item->key, item->key_length);
    next_addr += item->key_length;

    // 写入value
    ret &= g_skv_port->write(next_addr, &item->value_type, 1);
    next_addr += 1;
    ret &= g_skv_port->write(next_addr, &item->value_length, 1);
    next_addr += 1;
    ret &= g_skv_port->write(next_addr, (u8*)item->value, item->value_length);
    next_addr += item->value_length;

    // 写入crc16
    ret &= g_skv_port->write(next_addr, (u8*)&crc, 2);
    next_addr += 2;

    if(!ret){
        // 有1个失败就回滚
        return -1;
    }

    // 更新next_addr
    skv->next_addr = next_addr;
    skv->num++;
    
    return 0;
}

// 读取键值对
i32 skv_get_item(skv_t* skv, skv_kv_item_t* item) 
{
    u32 addr = skv->start_addr + 32; // header区32字节
    while (addr < skv->next_addr) {
        u8 item_magic = 0;
        // 读取item magic number
        g_skv_port->read(addr, &item_magic, 1);
        if (item_magic != SKV_ITEM_MAGIC) {
            // 非法item，跳过
            break;
        }
        addr += 1;
        // 读取key长度
        u8 key_length = 0;
        g_skv_port->read(addr, &key_length, 1);
        addr += 1;
        // 读取key
        char key_buf[SKV_MAX_KEY_LEN + 1] = {0};
        g_skv_port->read(addr, (u8*)key_buf, key_length);
        addr += key_length;

        // 比较key
        if (strcmp(key_buf, item->key) == 0) {
            // 找到对应key，读取value
            // 读取value类型
            g_skv_port->read(addr, &item->value_type, 1);
            addr += 1;
            // 读取value长度
            g_skv_port->read(addr, &item->value_length, 1);
            addr += 1;
            // 读取value
            g_skv_port->read(addr, (u8*)item->value, item->value_length);
            addr += item->value_length;
            // 跳过crc16
            addr += 2;
            return 0;
        } else {
            // 跳过value类型和长度及value和crc16
            u8 value_type = 0;
            g_skv_port->read(addr, &value_type, 1);
            addr += 1;
            u8 value_length = 0;
            g_skv_port->read(addr, &value_length, 1);
            addr += 1 + value_length + 2; // value + crc16
        }
    }
    // 未找到
    return SKV_ERR_NOT_FOUND;
}

i32 skv_put_i32(skv_t* skv, const char* key, i32 value)
{
    return 0;
}

#if TEST_ENABLE

#    include "../em_test/test.h"
#    include <string.h>

TEST_CASE(skv_basic)
{
}


#endif
