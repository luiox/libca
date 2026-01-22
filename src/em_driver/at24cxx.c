#include "at24cxx.h"
#include "../em_base/debug.h"

static const at24cxx_port_t* g_at24cxx_port = NULL;

void at24cxx_bind_port(const at24cxx_port_t* port)
{
    g_at24cxx_port = port;
}

bool at24cxx_port_is_registered(void)
{
    return g_at24cxx_port != NULL;
}

#define I2C_READ 1
#define I2C_WRITE 0

#define I2C_MEM_ADDR_SIZE_8BIT 0x01
#define I2C_MEM_ADDR_SIZE_16BIT 0x010

// 1010 0000
#define AT24CXX_BASIC_ADDR 0xA0

#define calc_need_byte(bit_size) ((bit_size + 7) / 8)

/**
 * @brief 获取at24cxx的容量
 *
 * @param type 类型
 * @return u32 位数
 */
static u32 at24cxx_get_mem_size(at24cxx_type_t type)
{
    switch (type) {
    case AT24C01: return 1 * 1024;
    case AT24C02: return 2 * 1024;
    case AT24C04: return 4 * 1024;
    case AT24C08: return 8 * 1024;
    case AT24C16: return 16 * 1024;
    case AT24C32: return 32 * 1024;
    case AT24C64: return 64 * 1024;
    case AT24C128: return 128 * 1024;
    case AT24C256: return 256 * 1024;
    case AT24C512: return 512 * 1024;
    default: debug_print("[at24cxx] error: unsupport type, type:%d\n", type); return 0;
    }
}

/**
 * @brief 根据类型获取地址大小
 *
 * @param type 类型
 * @return u8 地址的位数
 */
static u8 at24cxx_get_mem_addr_size(at24cxx_type_t type)
{
    switch (type) {
    case AT24C01: return 7;
    case AT24C02: return 8;
    case AT24C04: return 8;   // 这个实际是9，因为这个位会放在器件地址去，所以这个地方写8位
    case AT24C08: return 8;    // 这个实际是10
    case AT24C16: return 8;    // 这个实际是11
    case AT24C32: return 12;   // 这个实际是12
    case AT24C64: return 13;
    case AT24C128: return 14;
    case AT24C256: return 15;
    case AT24C512: return 16;
    default: debug_print("[at24cxx] error: unsupport type, type:%d\n", type); return 0;
    }
}

/**
 * @brief 获得页的数量
 *
 * @param type
 * @return u16
 */
static u16 at24cxx_get_page_size(at24cxx_type_t type)
{
    switch (type) {
    case AT24C02:
    case AT24C04: return 32;
    case AT24C08: return 64;
    case AT24C16:
    case AT24C32: return 128;
    case AT24C64:
    case AT24C128:
    case AT24C256:
    case AT24C512: return 512;
    default: debug_print("[at24cxx] error: unsupport type, type:%d\n", type); return 0;
    }
}

/**
 * @brief 获得一页有多少字节数
 *
 * @param type
 * @return u8
 */
static u8 at24cxx_get_page_bytes(at24cxx_type_t type)
{
    switch (type) {
    case AT24C02: return 8;
    case AT24C04:
    case AT24C08:
    case AT24C16: return 16;
    case AT24C32: return 32;
    case AT24C64:
    case AT24C128:
    case AT24C256: return 64;
    case AT24C512: return 128;
    default: debug_print("[at24cxx] error: unsupport type, type:%d\n", type); return 0;
    }
}

void at24cxx_init(at24cxx_t* self, at24cxx_type_t type, u8 a0, u8 a1, u8 a2, void* hi2c)
{
    self->type = type;
    self->hi2c = hi2c;

    self->mem_addr_bytes = calc_need_byte(at24cxx_get_mem_addr_size(self->type));
    self->mem_size       = at24cxx_get_mem_size(self->type);
    // 计算地址
    // 先给默认的基础地址
    self->dev_addr_base = AT24CXX_BASIC_ADDR;
    switch (self->type) {
    case AT24C02:
    case AT24C32:
    case AT24C64:
    case AT24C256:
        // 1010 a2 a2 a0 rw
        self->dev_addr_base = (AT24CXX_BASIC_ADDR | (a2 << 3) | (a1 << 2) | (a0 << 1));
        break;
    case AT24C04:
        // 1010 a2 a1 p0 rw
        // p0后面读写的时候再加
        self->dev_addr_base = (AT24CXX_BASIC_ADDR | (a2 << 3) | (a1 << 2));
        break;
    case AT24C08:
        // 1010 a2 p1 p0 rw
        self->dev_addr_base = (AT24CXX_BASIC_ADDR | (a2 << 2));
        break;
    case AT24C16:
        // 1010 p2 p1 p0 rw
        self->dev_addr_base = (AT24CXX_BASIC_ADDR);
        break;
    default: debug_print("[at24cxx] error: unsupport type, type:%d\n", type); break;
    }
}

void at24cxx_write_byte(at24cxx_t* self, u16 addr, u8 data)
{
    if (!g_at24cxx_port) {
        debug_print("[at24cxx] port not registered\n");
        return;
    }

    switch (self->type) {
    case AT24C02:
    case AT24C32:
    case AT24C64:
        g_at24cxx_port->i2c_write(self->hi2c,
                                  self->dev_addr_base | I2C_WRITE,
                                  addr,
                                  I2C_MEM_ADDR_SIZE_8BIT,
                                  &data,
                                  1,
                                  1000);
        break;
    case AT24C256:
        g_at24cxx_port->i2c_write(self->hi2c,
                                  self->dev_addr_base | I2C_WRITE,
                                  addr,
                                  I2C_MEM_ADDR_SIZE_16BIT,
                                  &data,
                                  1,
                                  1000);
        break;
    case AT24C04:
        // 1010 a2 a1 p0 rw
        g_at24cxx_port->i2c_write(self->hi2c,
                                  self->dev_addr_base | ((addr & 0x0100) >> 7) | I2C_WRITE,
                                  addr,
                                  I2C_MEM_ADDR_SIZE_8BIT,
                                  &data,
                                  1,
                                  1000);

        break;
    case AT24C08:
        // 1010 a2 p1 p0 rw
        g_at24cxx_port->i2c_write(
            self->hi2c,
            self->dev_addr_base | ((addr & 0x0200) >> 7) | ((addr & 0x0100) >> 7) | I2C_WRITE,
            addr,
            I2C_MEM_ADDR_SIZE_8BIT,
            &data,
            1,
            1000);
        break;
    case AT24C16:
        // 1010 p2 p1 p0 rw
        g_at24cxx_port->i2c_write(self->hi2c,
                                  self->dev_addr_base | ((addr & 0x0400) >> 7) |
                                      ((addr & 0x0200) >> 7) | ((addr & 0x0100) >> 7) | I2C_WRITE,
                                  addr,
                                  I2C_MEM_ADDR_SIZE_8BIT,
                                  &data,
                                  1,
                                  1000);
        break;
    default: debug_print("[at24cxx] error: unsupport type, type:%d\n", self->type); break;
    }
}

void at24cxx_read_byte(at24cxx_t* self, u16 addr, u8* data)
{
    if (!g_at24cxx_port) {
        debug_print("[at24cxx] port not registered\n");
        return;
    }

    switch (self->type) {
    case AT24C02:
    case AT24C32:
    case AT24C64:
        g_at24cxx_port->i2c_read(
            self->hi2c, self->dev_addr_base | I2C_READ, addr, I2C_MEM_ADDR_SIZE_8BIT, data, 1, 1000);
        break;
    case AT24C256:
        g_at24cxx_port->i2c_read(
            self->hi2c, self->dev_addr_base | I2C_READ, addr, I2C_MEM_ADDR_SIZE_16BIT, data, 1, 1000);
        break;
    case AT24C04:
        // 1010 a2 a1 p0 rw
        g_at24cxx_port->i2c_read(self->hi2c,
                                 self->dev_addr_base | ((addr & 0x0100) >> 7) | I2C_READ,
                                 addr,
                                 I2C_MEM_ADDR_SIZE_8BIT,
                                 data,
                                 1,
                                 1000);

        break;
    case AT24C08:
        // 1010 a2 p1 p0 rw
        g_at24cxx_port->i2c_read(
            self->hi2c,
            self->dev_addr_base | ((addr & 0x0200) >> 7) | ((addr & 0x0100) >> 7) | I2C_READ,
            addr,
            I2C_MEM_ADDR_SIZE_8BIT,
            data,
            1,
            1000);
        break;
    case AT24C16:
        // 1010 p2 p1 p0 rw
        g_at24cxx_port->i2c_read(self->hi2c,
                                 self->dev_addr_base | ((addr & 0x0400) >> 7) |
                                     ((addr & 0x0200) >> 7) | ((addr & 0x0100) >> 7) | I2C_READ,
                                 addr,
                                 I2C_MEM_ADDR_SIZE_8BIT,
                                 data,
                                 1,
                                 1000);
        break;
    default: debug_print("[at24cxx] error: unsupport type, type:%d\n", self->type); break;
    }
}
