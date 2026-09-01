#include "bh1750.h"
#include <em_base/debug.h>

////////////////////////////////////////////////////////////////////////////////

#if (LIBCA_BH1750_PORT_MODE == LIBCA_BH1750_PORT_MODE_EXTERN)

#    define BH1750_I2C_WRITE(hi2c, dev_addr, mem_addr, mem_addr_size, data, data_size, timeout) \
        port_bh1750_i2c_write(                                                                  \
            (hi2c), (dev_addr), (mem_addr), (mem_addr_size), (data), (data_size), (timeout))
#    define BH1750_I2C_READ(hi2c, dev_addr, mem_addr, mem_addr_size, data, data_size, timeout) \
        port_bh1750_i2c_read(                                                                  \
            (hi2c), (dev_addr), (mem_addr), (mem_addr_size), (data), (data_size), (timeout))

#elif (LIBCA_BH1750_PORT_MODE == LIBCA_BH1750_PORT_MODE_DYNAMIC)

static const bh1750_port_t* g_bh1750_port = NULL;

#    define BH1750_I2C_WRITE(hi2c, dev_addr, mem_addr, mem_addr_size, data, data_size, timeout) \
        g_bh1750_port->i2c_write(                                                               \
            (hi2c), (dev_addr), (mem_addr), (mem_addr_size), (data), (data_size), (timeout))
#    define BH1750_I2C_READ(hi2c, dev_addr, mem_addr, mem_addr_size, data, data_size, timeout) \
        g_bh1750_port->i2c_read(                                                               \
            (hi2c), (dev_addr), (mem_addr), (mem_addr_size), (data), (data_size), (timeout))

#else
#    error "Invalid BH1750 port mode"
#endif

#if (LIBCA_BH1750_PORT_MODE == LIBCA_BH1750_PORT_MODE_DYNAMIC)
void bh1750_bind_port(const bh1750_port_t* port)
{
    g_bh1750_port = port;
}

bool bh1750_port_is_registered(void)
{
    return g_bh1750_port != NULL;
}
#endif

////////////////////////////////////////////////////////////////////////////////

#define bh1750_send_cmd(self, cmd) \
    BH1750_I2C_WRITE((self)->hi2c, BH1750_ADDR_WRITE, 0, 0, (u8*)&(cmd), 1, 0xFFFF)
#define bh1750_read_dat(self, dat) \
    BH1750_I2C_READ((self)->hi2c, BH1750_ADDR_READ, 0, 0, (dat), 2, 0xFFFF)

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
    i32 ret = bh1750_send_cmd(self, mode);
    if (ret != 0) {
        debug_print("[bh1750] i2c write fail, ret:%d\n", ret);
        return BH1750_ERR_I2C_FAIL;
    }

    return BH1750_OK;
}

i32 bh1750_read_lux(bh1750_t* self, u16* lux)
{
    u8 dat[2] = {0};

    if (bh1750_read_dat(self, dat) != 0) {
        debug_print("[bh1750] i2c read fail\n");
        return BH1750_ERR_I2C_FAIL;
    }

    *lux = bh1750_dat2lux(dat);

    return BH1750_OK;
}