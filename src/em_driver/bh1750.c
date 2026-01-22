#include "bh1750.h"

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
static uint16_t bh1750_dat2lux(uint8_t* dat)
{
    uint16_t lux = 0;
    
    lux = dat[0];
    lux <<= 8;
    lux += dat[1];
    lux = (int)(lux / 1.2);
	
    return lux;
}

void bh1750_init(bh1750_t* self)
{
    // nothing to do
}

i32 bh1750_start(bh1750_t* self, bh1750_mode_t mode)
{
    return bh1750_send_cmd(self, mode);
} 

i32 bh1750_read_lux(bh1750_t* self, u16 *lux)
{
    uint8_t dat[2] = {0};

    if (bh1750_read_dat(self, dat) != 0) {
        return -1;
    }

    *lux = bh1750_dat2lux(dat);

    return 0;
} 
