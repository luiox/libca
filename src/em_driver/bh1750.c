#include "bh1750.h"
#include "../em_base/debug.h"

static const bh1750_port_t* g_bh1750_port = NULL;

void bh1750_bind_port(const bh1750_port_t* port)
{
    g_bh1750_port = port;
}

bool bh1750_port_is_registered(void)
{
    return g_bh1750_port != NULL;
}

// 辅助包装层，用宏实现，避免函数式调用开销
#define bh1750_send_cmd(self, cmd) g_bh1750_port->i2c_write(self->hi2c, BH1750_ADDR_WRITE, 0, 0, (uint8_t*)&cmd, 1, 0xFFFF)
#define bh1750_read_dat(self, dat) g_bh1750_port->i2c_read(self->hi2c, BH1750_ADDR_READ, 0, 0, dat, 2, 0xFFFF)

// 将数据转换为lux单位
static u16 bh1750_dat2lux(uint8_t* dat)
{
    u32 raw = ((u16)dat[0] << 8) | dat[1];
    return (u16)(raw * 5 / 6);
}

void bh1750_init(bh1750_t* self)
{
    // nothing to do
}

i32 bh1750_start(bh1750_t* self, bh1750_mode_t mode)
{
    if (!g_bh1750_port) {
        debug_print("[bh1750] port not registered\n");
        return BH1750_ERR_PORT_NOT_REGISTERED;
    }

    i32 ret = bh1750_send_cmd(self, mode);
    if (ret != 0) {
        debug_print("[bh1750] i2c write fail, ret:%d\n", ret);
        return BH1750_ERR_I2C_FAIL;
    }

    return BH1750_OK;
} 

i32 bh1750_read_lux(bh1750_t* self, u16 *lux)
{
    u8 dat[2] = {0};

    if (!g_bh1750_port) {
        debug_print("[bh1750] port not registered\n");
        return BH1750_ERR_PORT_NOT_REGISTERED;
    }

    if (bh1750_read_dat(self, dat) != 0) {
        debug_print("[bh1750] i2c read fail\n");
        return BH1750_ERR_I2C_FAIL;
    }

    *lux = bh1750_dat2lux(dat);

    return BH1750_OK;
} 
