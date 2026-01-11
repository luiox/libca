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
    if (!g_skv_port) {
        return -1;
    }
    if (!skv || !item || !item->key) {
        return -1;
    }
    if (item->key_length == 0 || item->key_length > SKV_MAX_KEY_LEN) {
        return -1;
    }
    if (item->value_length > SKV_MAX_VALUE_LEN) {
        return -1;
    }

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

    // 写入crc16（按小端序写入，便于跨平台一致）
    u8 crc_bytes[2];
    crc_bytes[0] = (u8)(crc & 0xFF);
    crc_bytes[1] = (u8)((crc >> 8) & 0xFF);
    ret &= g_skv_port->write(next_addr, crc_bytes, 2);
    next_addr += 2;

    if (!ret) {
        // 有1个失败就回滚
        return -1;
    }

    // 更新next_addr
    skv->next_addr = next_addr;
    skv->num++;

    // 持久化header到介质
    if (!skv_write_header(skv)) {
        // 如果写header失败，回滚内存状态（注意：闪存上的item已写，真实回滚比较复杂）
        skv->num--;
        skv->next_addr -= needed_size;
        return -1;
    }

    return 0;
}

// 读取键值对
i32 skv_get_item(skv_t* skv, skv_kv_item_t* item)
{
    u32 addr = skv->start_addr + 32;   // header区32字节
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
        }
        else {
            // 跳过value类型和长度及value和crc16
            u8 value_type = 0;
            g_skv_port->read(addr, &value_type, 1);
            addr += 1;
            u8 value_length = 0;
            g_skv_port->read(addr, &value_length, 1);
            addr += 1 + value_length + 2;   // value + crc16
        }
    }
    // 未找到
    return SKV_ERR_NOT_FOUND;
}

void skv_reset_storage(skv_t* skv)
{
    if (!skv) return;
    skv->num        = 0;
    skv->next_addr  = skv->start_addr + 32;   // header区32字节
    skv_write_header(skv);
}

// --- little-endian helpers ---
static void write_le_u16(u8* buf, u16 v) { buf[0] = (u8)(v & 0xFF); buf[1] = (u8)((v >> 8) & 0xFF); }
static void write_le_u32(u8* buf, u32 v) { buf[0] = (u8)(v & 0xFF); buf[1] = (u8)((v >> 8) & 0xFF); buf[2] = (u8)((v >> 16) & 0xFF); buf[3] = (u8)((v >> 24) & 0xFF); }
#ifdef HAS_INT64
static void write_le_u64(u8* buf, u64 v) { for (int i = 0; i < 8; ++i) buf[i] = (u8)((v >> (8 * i)) & 0xFF); }
static u64 read_le_u64(const u8* buf) { u64 v = 0; for (int i = 0; i < 8; ++i) v |= ((u64)buf[i]) << (8 * i); return v; }
#endif
static u16 read_le_u16(const u8* buf) { return (u16)buf[0] | ((u16)buf[1] << 8); }
static u32 read_le_u32(const u8* buf) { return (u32)buf[0] | ((u32)buf[1] << 8) | ((u32)buf[2] << 16) | ((u32)buf[3] << 24); }

// typed put helpers
static i32 skv_put_raw(skv_t* skv, const char* key, u8 value_type, const u8* data, u8 len)
{
    if (!key) return -1;
    skv_kv_item_t item;
    item.key_length = (u8)strlen(key);
    item.key = (char*)key;
    item.value_type = value_type;
    item.value_length = len;
    item.value = (void*)data;
    return skv_put_item(skv, &item);
}

i32 skv_put_u8(skv_t* skv, const char* key, u8 value)
{
    u8 buf[1] = {value};
    return skv_put_raw(skv, key, SKV_TYPE_U8, buf, 1);
}

i32 skv_get_u8(skv_t* skv, const char* key, u8* value)
{
    u8 buf[SKV_MAX_VALUE_LEN];
    skv_kv_item_t item;
    item.key = (char*)key;
    item.key_length = (u8)strlen(key);
    item.value = buf;
    if (skv_get_item(skv, &item) != 0) return SKV_ERR_NOT_FOUND;
    if (item.value_type != SKV_TYPE_U8 || item.value_length != 1) return -1;
    *value = buf[0];
    return 0;
}

i32 skv_put_u16(skv_t* skv, const char* key, u16 value)
{
    u8 buf[2]; write_le_u16(buf, value); return skv_put_raw(skv, key, SKV_TYPE_U16, buf, 2);
}

i32 skv_get_u16(skv_t* skv, const char* key, u16* value)
{
    u8 buf[SKV_MAX_VALUE_LEN]; skv_kv_item_t item; item.key = (char*)key; item.key_length = (u8)strlen(key); item.value = buf;
    if (skv_get_item(skv, &item) != 0) return SKV_ERR_NOT_FOUND;
    if (item.value_type != SKV_TYPE_U16 || item.value_length != 2) return -1;
    *value = read_le_u16(buf);
    return 0;
}

i32 skv_put_u32(skv_t* skv, const char* key, u32 value)
{
    u8 buf[4]; write_le_u32(buf, value); return skv_put_raw(skv, key, SKV_TYPE_U32, buf, 4);
}

i32 skv_get_u32(skv_t* skv, const char* key, u32* value)
{
    u8 buf[SKV_MAX_VALUE_LEN]; skv_kv_item_t item; item.key = (char*)key; item.key_length = (u8)strlen(key); item.value = buf;
    if (skv_get_item(skv, &item) != 0) return SKV_ERR_NOT_FOUND;
    if (item.value_type != SKV_TYPE_U32 || item.value_length != 4) return -1;
    *value = read_le_u32(buf);
    return 0;
}

#ifdef HAS_INT64
i32 skv_put_u64(skv_t* skv, const char* key, u64 value)
{
    u8 buf[8]; write_le_u64(buf, value); return skv_put_raw(skv, key, SKV_TYPE_U64, buf, 8);
}

i32 skv_get_u64(skv_t* skv, const char* key, u64* value)
{
    u8 buf[SKV_MAX_VALUE_LEN]; skv_kv_item_t item; item.key = (char*)key; item.key_length = (u8)strlen(key); item.value = buf;
    if (skv_get_item(skv, &item) != 0) return SKV_ERR_NOT_FOUND;
    if (item.value_type != SKV_TYPE_U64 || item.value_length != 8) return -1;
    *value = read_le_u64(buf);
    return 0;
}
#else
// u64 not available on this platform; u64 helpers are not provided
#endif

// signed variants
i32 skv_put_i8(skv_t* skv, const char* key, i8 value) { u8 buf[1] = {(u8)value}; return skv_put_raw(skv, key, SKV_TYPE_I8, buf, 1); }
i32 skv_get_i8(skv_t* skv, const char* key, i8* value) { u8 buf[1]; skv_kv_item_t item; item.key = (char*)key; item.key_length = (u8)strlen(key); item.value = buf; if (skv_get_item(skv, &item) != 0) return SKV_ERR_NOT_FOUND; if (item.value_type != SKV_TYPE_I8 || item.value_length != 1) return -1; *value = (i8)buf[0]; return 0; }

i32 skv_put_i16(skv_t* skv, const char* key, i16 value) { u8 buf[2]; write_le_u16(buf, (u16)value); return skv_put_raw(skv, key, SKV_TYPE_I16, buf, 2); }
i32 skv_get_i16(skv_t* skv, const char* key, i16* value) { u8 buf[SKV_MAX_VALUE_LEN]; skv_kv_item_t item; item.key = (char*)key; item.key_length = (u8)strlen(key); item.value = buf; if (skv_get_item(skv, &item) != 0) return SKV_ERR_NOT_FOUND; if (item.value_type != SKV_TYPE_I16 || item.value_length != 2) return -1; *value = (i16)read_le_u16(buf); return 0; }

i32 skv_put_i32(skv_t* skv, const char* key, i32 value) { u8 buf[4]; write_le_u32(buf, (u32)value); return skv_put_raw(skv, key, SKV_TYPE_I32, buf, 4); }
i32 skv_get_i32(skv_t* skv, const char* key, i32* value) { u8 buf[SKV_MAX_VALUE_LEN]; skv_kv_item_t item; item.key = (char*)key; item.key_length = (u8)strlen(key); item.value = buf; if (skv_get_item(skv, &item) != 0) return SKV_ERR_NOT_FOUND; if (item.value_type != SKV_TYPE_I32 || item.value_length != 4) return -1; *value = (i32)read_le_u32(buf); return 0; }

// floats
i32 skv_put_f32(skv_t* skv, const char* key, f32 value)
{
    u8 buf[4]; u32 v; memcpy(&v, &value, 4); write_le_u32(buf, v); return skv_put_raw(skv, key, SKV_TYPE_F32, buf, 4);
}

i32 skv_get_f32(skv_t* skv, const char* key, f32* value)
{
    u8 buf[SKV_MAX_VALUE_LEN]; skv_kv_item_t item; item.key = (char*)key; item.key_length = (u8)strlen(key); item.value = buf;
    if (skv_get_item(skv, &item) != 0) return SKV_ERR_NOT_FOUND;
    if (item.value_type != SKV_TYPE_F32 || item.value_length != 4) return -1;
    u32 v = read_le_u32(buf); memcpy(value, &v, 4); return 0;
}

i32 skv_put_f64(skv_t* skv, const char* key, f64 value)
{
    u8 buf[8];
    if (is_little_endian()) {
        memcpy(buf, &value, 8);
    } else {
        u8 tmp[8]; memcpy(tmp, &value, 8);
        for (int i = 0; i < 8; ++i) buf[i] = tmp[7 - i];
    }
    return skv_put_raw(skv, key, SKV_TYPE_F64, buf, 8);
}

i32 skv_get_f64(skv_t* skv, const char* key, f64* value)
{
    u8 buf[SKV_MAX_VALUE_LEN]; skv_kv_item_t item; item.key = (char*)key; item.key_length = (u8)strlen(key); item.value = buf;
    if (skv_get_item(skv, &item) != 0) return SKV_ERR_NOT_FOUND;
    if (item.value_type != SKV_TYPE_F64 || item.value_length != 8) return -1;
    if (is_little_endian()) {
        memcpy(value, buf, 8);
    } else {
        u8 tmp[8]; for (int i = 0; i < 8; ++i) tmp[i] = buf[7 - i]; memcpy(value, tmp, 8);
    }
    return 0;
}

// string/blob
i32 skv_put_string(skv_t* skv, const char* key, const char* str)
{
    if (!str) return -1;
    u8 len = (u8)strlen(str);
    return skv_put_raw(skv, key, SKV_TYPE_STRING, (const u8*)str, len);
}

i32 skv_get_string(skv_t* skv, const char* key, char* buf, u8 buf_len)
{
    u8 tmp[SKV_MAX_VALUE_LEN]; skv_kv_item_t item; item.key = (char*)key; item.key_length = (u8)strlen(key); item.value = tmp;
    if (skv_get_item(skv, &item) != 0) return SKV_ERR_NOT_FOUND;
    u8 copy_len = item.value_length < (u8)(buf_len - 1) ? item.value_length : (u8)(buf_len - 1);
    memcpy(buf, tmp, copy_len);
    buf[copy_len] = '\0';
    return 0;
}

i32 skv_put_blob(skv_t* skv, const char* key, const u8* data, u8 len)
{
    if (!data) return -1;
    return skv_put_raw(skv, key, SKV_TYPE_BLOB, data, len);
}

i32 skv_get_blob(skv_t* skv, const char* key, u8* buf, u8 buf_len, u8* out_len)
{
    u8 tmp[SKV_MAX_VALUE_LEN]; skv_kv_item_t item; item.key = (char*)key; item.key_length = (u8)strlen(key); item.value = tmp;
    if (skv_get_item(skv, &item) != 0) return SKV_ERR_NOT_FOUND;
    if (item.value_length > buf_len) return -1;
    memcpy(buf, tmp, item.value_length);
    if (out_len) *out_len = item.value_length;
    return 0;
}

// --- or_default helpers ---
i32 skv_get_u8_or_default(skv_t* skv, const char* key, u8* value, u8 default_value)
{
    if (!key || !value) return SKV_ERR_INVALID_PARAM;
    i32 r = skv_get_u8(skv, key, value);
    if (r == 0) return SKV_OK;
    i32 pr = skv_put_u8(skv, key, default_value);
    if (pr == 0) { *value = default_value; return (r == SKV_ERR_NOT_FOUND) ? SKV_RET_DEFAULT_WRITTEN_NOT_FOUND : SKV_RET_DEFAULT_WRITTEN_READ_FAILED; }
    return SKV_ERR_WRITE_FAILED;
}

i32 skv_get_u16_or_default(skv_t* skv, const char* key, u16* value, u16 default_value)
{
    if (!key || !value) return SKV_ERR_INVALID_PARAM;
    i32 r = skv_get_u16(skv, key, value);
    if (r == 0) return SKV_OK;
    i32 pr = skv_put_u16(skv, key, default_value);
    if (pr == 0) { *value = default_value; return (r == SKV_ERR_NOT_FOUND) ? SKV_RET_DEFAULT_WRITTEN_NOT_FOUND : SKV_RET_DEFAULT_WRITTEN_READ_FAILED; }
    return SKV_ERR_WRITE_FAILED;
}

i32 skv_get_u32_or_default(skv_t* skv, const char* key, u32* value, u32 default_value)
{
    if (!key || !value) return SKV_ERR_INVALID_PARAM;
    i32 r = skv_get_u32(skv, key, value);
    if (r == 0) return SKV_OK;
    i32 pr = skv_put_u32(skv, key, default_value);
    if (pr == 0) { *value = default_value; return (r == SKV_ERR_NOT_FOUND) ? SKV_RET_DEFAULT_WRITTEN_NOT_FOUND : SKV_RET_DEFAULT_WRITTEN_READ_FAILED; }
    return SKV_ERR_WRITE_FAILED;
}

#ifdef HAS_INT64
i32 skv_get_u64_or_default(skv_t* skv, const char* key, u64* value, u64 default_value)
{
    if (!key || !value) return SKV_ERR_INVALID_PARAM;
    i32 r = skv_get_u64(skv, key, value);
    if (r == 0) return SKV_OK;
    i32 pr = skv_put_u64(skv, key, default_value);
    if (pr == 0) { *value = default_value; return (r == SKV_ERR_NOT_FOUND) ? SKV_RET_DEFAULT_WRITTEN_NOT_FOUND : SKV_RET_DEFAULT_WRITTEN_READ_FAILED; }
    return SKV_ERR_WRITE_FAILED;
}
#endif

i32 skv_get_i8_or_default(skv_t* skv, const char* key, i8* value, i8 default_value)
{
    if (!key || !value) return SKV_ERR_INVALID_PARAM;
    i32 r = skv_get_i8(skv, key, value);
    if (r == 0) return SKV_OK;
    i32 pr = skv_put_i8(skv, key, default_value);
    if (pr == 0) { *value = default_value; return (r == SKV_ERR_NOT_FOUND) ? SKV_RET_DEFAULT_WRITTEN_NOT_FOUND : SKV_RET_DEFAULT_WRITTEN_READ_FAILED; }
    return SKV_ERR_WRITE_FAILED;
}

i32 skv_get_i16_or_default(skv_t* skv, const char* key, i16* value, i16 default_value)
{
    if (!key || !value) return SKV_ERR_INVALID_PARAM;
    i32 r = skv_get_i16(skv, key, value);
    if (r == 0) return SKV_OK;
    i32 pr = skv_put_i16(skv, key, default_value);
    if (pr == 0) { *value = default_value; return (r == SKV_ERR_NOT_FOUND) ? SKV_RET_DEFAULT_WRITTEN_NOT_FOUND : SKV_RET_DEFAULT_WRITTEN_READ_FAILED; }
    return SKV_ERR_WRITE_FAILED;
}

i32 skv_get_i32_or_default(skv_t* skv, const char* key, i32* value, i32 default_value)
{
    if (!key || !value) return SKV_ERR_INVALID_PARAM;
    i32 r = skv_get_i32(skv, key, value);
    if (r == 0) return SKV_OK;
    i32 pr = skv_put_i32(skv, key, default_value);
    if (pr == 0) { *value = default_value; return (r == SKV_ERR_NOT_FOUND) ? SKV_RET_DEFAULT_WRITTEN_NOT_FOUND : SKV_RET_DEFAULT_WRITTEN_READ_FAILED; }
    return SKV_ERR_WRITE_FAILED;
}

i32 skv_get_f32_or_default(skv_t* skv, const char* key, f32* value, f32 default_value)
{
    if (!key || !value) return SKV_ERR_INVALID_PARAM;
    i32 r = skv_get_f32(skv, key, value);
    if (r == 0) return SKV_OK;
    i32 pr = skv_put_f32(skv, key, default_value);
    if (pr == 0) { *value = default_value; return (r == SKV_ERR_NOT_FOUND) ? SKV_RET_DEFAULT_WRITTEN_NOT_FOUND : SKV_RET_DEFAULT_WRITTEN_READ_FAILED; }
    return SKV_ERR_WRITE_FAILED;
}

i32 skv_get_f64_or_default(skv_t* skv, const char* key, f64* value, f64 default_value)
{
    if (!key || !value) return SKV_ERR_INVALID_PARAM;
    i32 r = skv_get_f64(skv, key, value);
    if (r == 0) return SKV_OK;
    i32 pr = skv_put_f64(skv, key, default_value);
    if (pr == 0) { *value = default_value; return (r == SKV_ERR_NOT_FOUND) ? SKV_RET_DEFAULT_WRITTEN_NOT_FOUND : SKV_RET_DEFAULT_WRITTEN_READ_FAILED; }
    return SKV_ERR_WRITE_FAILED;
}

i32 skv_get_string_or_default(skv_t* skv, const char* key, char* buf, u8 buf_len, const char* default_value)
{
    if (!key || !buf || !default_value) return SKV_ERR_INVALID_PARAM;
    i32 r = skv_get_string(skv, key, buf, buf_len);
    if (r == 0) return SKV_OK;
    i32 pr = skv_put_string(skv, key, default_value);
    if (pr == 0) { // copy default into buf
        u8 copy_len = (u8)strlen(default_value);
        if (copy_len >= buf_len) copy_len = buf_len - 1;
        memcpy(buf, default_value, copy_len);
        buf[copy_len] = '\0';
        return (r == SKV_ERR_NOT_FOUND) ? SKV_RET_DEFAULT_WRITTEN_NOT_FOUND : SKV_RET_DEFAULT_WRITTEN_READ_FAILED;
    }
    return SKV_ERR_WRITE_FAILED;
}

i32 skv_get_blob_or_default(skv_t* skv, const char* key, u8* buf, u8 buf_len, const u8* default_data, u8 default_len)
{
    if (!key || !buf || (!default_data && default_len != 0)) return SKV_ERR_INVALID_PARAM;
    u8 out_len = 0;
    i32 r = skv_get_blob(skv, key, buf, buf_len, &out_len);
    if (r == 0) return SKV_OK;
    if (default_len > buf_len) return SKV_ERR_INVALID_PARAM;
    i32 pr = skv_put_blob(skv, key, default_data, default_len);
    if (pr == 0) {
        if (default_len) memcpy(buf, default_data, default_len);
        if (default_len < buf_len) buf[default_len] = 0;
        if (out_len) *(&out_len) = default_len; // not used by caller here
        return (r == SKV_ERR_NOT_FOUND) ? SKV_RET_DEFAULT_WRITTEN_NOT_FOUND : SKV_RET_DEFAULT_WRITTEN_READ_FAILED;
    }
    return SKV_ERR_WRITE_FAILED;
}



#if TEST_ENABLE

#    include "../em_test/test.h"
#    include <string.h>
#    include <stdio.h>

FILE* g_test_skv_file = NULL;

static bool test_skv_read(u32 addr, u8* buf, u32 len) {
    if (!g_test_skv_file) return false;
    if (fseek(g_test_skv_file, (long)addr, SEEK_SET) != 0) return false;
    size_t r = fread(buf, 1, (size_t)len, g_test_skv_file);
    return r == len;
}

static bool test_skv_write(u32 addr, u8* buf, u32 len) {
    if (!g_test_skv_file) return false;
    if (fseek(g_test_skv_file, (long)addr, SEEK_SET) != 0) return false;
    size_t w = fwrite(buf, 1, (size_t)len, g_test_skv_file);
    fflush(g_test_skv_file);
    return w == len;
}

static skv_port_t test_skv_port = {
    .read  = test_skv_read,
    .write = test_skv_write,
};

TEST_CASE(skv_basic) {
    // 使用一个临时二进制文件模拟eeprom
    g_test_skv_file = fopen("test_skv.bin", "wb+");
    TEST_ASSERT_NOT_NULL(g_test_skv_file);

    // 分配1KB作为模拟存储
    fseek(g_test_skv_file, 1024 - 1, SEEK_SET);
    fputc(0, g_test_skv_file);
    fflush(g_test_skv_file);

    skv_bind_port(&test_skv_port);

    skv_t skv;
    skv_init(&skv, 0);
    skv.total_size = 1024;
    skv.next_addr = skv.start_addr + 32;
    TEST_ASSERT(skv_write_header(&skv));

    // 写入一个i32值
    i32 v = 0x12345678;
    TEST_ASSERT_EQUAL_INT(0, skv_put_i32(&skv, "mykey", v));

    // 确认header已持久化
    skv_t skv2;
    skv2.start_addr = 0;
    TEST_ASSERT(skv_read_header(&skv2));
    TEST_ASSERT_EQUAL_UINT(1u, skv2.num);

    // 读取回刚才的数据
    skv_kv_item_t q;
    memset(&q, 0, sizeof(q));
    q.key = "mykey";
    q.key_length = (u8)strlen(q.key);
    i32 got = 0;
    q.value = &got;
    TEST_ASSERT_EQUAL_INT(0, skv_get_item(&skv, &q));
    TEST_ASSERT_EQUAL_UINT((unsigned int)SKV_TYPE_I32, (unsigned int)q.value_type);
    TEST_ASSERT_EQUAL_INT(v, got);

    // 未找到key
    q.key = "nokey";
    q.key_length = (u8)strlen(q.key);
    TEST_ASSERT_EQUAL_INT(SKV_ERR_NOT_FOUND, skv_get_item(&skv, &q));

    fclose(g_test_skv_file);
    g_test_skv_file = NULL;
    remove("test_skv.bin");
}

TEST_CASE(skv_types_put_get) {
    // 使用一个临时二进制文件模拟eeprom
    g_test_skv_file = fopen("test_skv_types_put_get.bin", "wb+");
    TEST_ASSERT_NOT_NULL(g_test_skv_file);

    // 分配2KB作为模拟存储
    fseek(g_test_skv_file, 2048 - 1, SEEK_SET);
    fputc(0, g_test_skv_file);
    fflush(g_test_skv_file);

    skv_bind_port(&test_skv_port);

    skv_t skv;
    skv_init(&skv, 0);
    skv.total_size = 2048;
    skv.next_addr = skv.start_addr + 32;
    TEST_ASSERT(skv_write_header(&skv));

    // 写入各种类型
    TEST_ASSERT_EQUAL_INT(0, skv_put_u8(&skv, "u8", 0xAB));
    TEST_ASSERT_EQUAL_INT(0, skv_put_u16(&skv, "u16", 0x1234));
    TEST_ASSERT_EQUAL_INT(0, skv_put_u32(&skv, "u32", 0x89ABCDEF));
#ifdef HAS_INT64
    TEST_ASSERT_EQUAL_INT(0, skv_put_u64(&skv, "u64", (u64)0x1122334455667788ULL));
#endif
    TEST_ASSERT_EQUAL_INT(0, skv_put_i8(&skv, "i8", (i8)-5));
    TEST_ASSERT_EQUAL_INT(0, skv_put_i16(&skv, "i16", (i16)-12345));
    TEST_ASSERT_EQUAL_INT(0, skv_put_i32(&skv, "i32", (i32)-123456789));
    TEST_ASSERT_EQUAL_INT(0, skv_put_f32(&skv, "f32", (f32)3.14159f));
    TEST_ASSERT_EQUAL_INT(0, skv_put_f64(&skv, "f64", (f64)2.718281828459045));
    TEST_ASSERT_EQUAL_INT(0, skv_put_string(&skv, "str", "hello world"));
    const u8 blob_data[] = {0x01, 0x02, 0xFF, 0x00};
    TEST_ASSERT_EQUAL_INT(0, skv_put_blob(&skv, "blob", blob_data, sizeof(blob_data)));

    // 读取并验证
    u8 u8v; TEST_ASSERT_EQUAL_INT(0, skv_get_u8(&skv, "u8", &u8v)); TEST_ASSERT_EQUAL_UINT(0xABu, (unsigned int)u8v);
    u16 u16v; TEST_ASSERT_EQUAL_INT(0, skv_get_u16(&skv, "u16", &u16v)); TEST_ASSERT_EQUAL_UINT(0x1234u, (unsigned int)u16v);
    u32 u32v; TEST_ASSERT_EQUAL_INT(0, skv_get_u32(&skv, "u32", &u32v)); TEST_ASSERT_EQUAL_UINT(0x89ABCDEFu, (unsigned int)u32v);
#ifdef HAS_INT64
    u64 u64v; TEST_ASSERT_EQUAL_INT(0, skv_get_u64(&skv, "u64", &u64v)); TEST_ASSERT_EQUAL_UINT(0x1122334455667788ULL, u64v);
#endif
    i8 i8v; TEST_ASSERT_EQUAL_INT(0, skv_get_i8(&skv, "i8", &i8v)); TEST_ASSERT_EQUAL_INT(-5, (int)i8v);
    i16 i16v; TEST_ASSERT_EQUAL_INT(0, skv_get_i16(&skv, "i16", &i16v)); TEST_ASSERT_EQUAL_INT(-12345, (int)i16v);
    i32 i32v; TEST_ASSERT_EQUAL_INT(0, skv_get_i32(&skv, "i32", &i32v)); TEST_ASSERT_EQUAL_INT(-123456789, (int)i32v);
    f32 f32v; TEST_ASSERT_EQUAL_INT(0, skv_get_f32(&skv, "f32", &f32v)); TEST_ASSERT_EQUAL_FLOAT(3.14159f, f32v, DEFAULT_EPSILON);
    f64 f64v; TEST_ASSERT_EQUAL_INT(0, skv_get_f64(&skv, "f64", &f64v)); TEST_ASSERT_EQUAL_FLOAT(2.718281828459045, f64v, DEFAULT_EPSILON);
    char strbuf[64]; TEST_ASSERT_EQUAL_INT(0, skv_get_string(&skv, "str", strbuf, sizeof(strbuf))); TEST_ASSERT_EQUAL_STRING("hello world", strbuf);
    u8 out_blob[16]; u8 out_len = 0; TEST_ASSERT_EQUAL_INT(0, skv_get_blob(&skv, "blob", out_blob, sizeof(out_blob), &out_len)); TEST_ASSERT_EQUAL_UINT(sizeof(blob_data), out_len); TEST_ASSERT_EQUAL_MEMORY(blob_data, out_blob, out_len);

    fclose(g_test_skv_file);
    g_test_skv_file = NULL;
    remove("test_skv_types_put_get.bin");
}

TEST_CASE(skv_or_default_missing) {
    g_test_skv_file = fopen("test_skv_or_default_missing.bin", "wb+");
    TEST_ASSERT_NOT_NULL(g_test_skv_file);
    fseek(g_test_skv_file, 1024 - 1, SEEK_SET);
    fputc(0, g_test_skv_file);
    fflush(g_test_skv_file);

    skv_bind_port(&test_skv_port);
    skv_t skv;
    skv_init(&skv, 0);
    skv.total_size = 1024;
    skv.next_addr = skv.start_addr + 32;
    TEST_ASSERT(skv_write_header(&skv));

    u32 dflt = 0xDEADBEEFu;
    u32 got = 0;
    TEST_ASSERT_EQUAL_INT(SKV_RET_DEFAULT_WRITTEN_NOT_FOUND, skv_get_u32_or_default(&skv, "missing_u32", &got, dflt));
    TEST_ASSERT_EQUAL_UINT(dflt, got);

    // ensure it's persisted
    skv_t skv2; skv2.start_addr = 0; TEST_ASSERT(skv_read_header(&skv2)); TEST_ASSERT_EQUAL_UINT(1u, skv2.num);

    fclose(g_test_skv_file);
    g_test_skv_file = NULL;
    remove("test_skv_or_default_missing.bin");
}

TEST_CASE(skv_or_default_write_failure) {
    g_test_skv_file = fopen("test_skv_or_default_write_failure.bin", "wb+");
    TEST_ASSERT_NOT_NULL(g_test_skv_file);
    fseek(g_test_skv_file, 1024 - 1, SEEK_SET);
    fputc(0, g_test_skv_file);
    fflush(g_test_skv_file);

    skv_bind_port(&test_skv_port);
    skv_t skv;
    skv_init(&skv, 0);
    skv.total_size = 1024;
    skv.next_addr = skv.start_addr + 32;
    TEST_ASSERT(skv_write_header(&skv));

    // simulate write failure by nulling the file pointer used by the port
    g_test_skv_file = NULL;
    u8 val = 0;
    TEST_ASSERT_EQUAL_INT(SKV_ERR_WRITE_FAILED, skv_get_u8_or_default(&skv, "any", &val, 0x7F));

    // reopen and cleanup
    g_test_skv_file = fopen("test_skv_or_default_write_failure.bin", "rb+");
    TEST_ASSERT_NOT_NULL(g_test_skv_file);
    fclose(g_test_skv_file);
    g_test_skv_file = NULL;
    remove("test_skv_or_default_write_failure.bin");
}

#endif
