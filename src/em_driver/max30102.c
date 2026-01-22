#include "max30102.h"
#include "../em_base/debug.h"
#include <stddef.h>

static const max30102_port_t* g_port = NULL;

void max30102_bind_port(const max30102_port_t* port)
{
    g_port = port;
}

bool max30102_port_is_registered(void)
{
    return g_port != NULL;
}


// Register addresses
#define MAX30102_REG_INTR_STATUS_1   0x00
#define MAX30102_REG_INTR_STATUS_2   0x01
#define MAX30102_REG_INTR_ENABLE_1   0x02
#define MAX30102_REG_INTR_ENABLE_2   0x03
#define MAX30102_REG_FIFO_WR_PTR     0x04
#define MAX30102_REG_OVF_COUNTER      0x05
#define MAX30102_REG_FIFO_RD_PTR     0x06
#define MAX30102_REG_FIFO_DATA       0x07
#define MAX30102_REG_FIFO_CONFIG     0x08
#define MAX30102_REG_MODE_CONFIG     0x09
#define MAX30102_REG_SPO2_CONFIG     0x0A
#define MAX30102_REG_LED1_PA         0x0C
#define MAX30102_REG_LED2_PA         0x0D
#define MAX30102_REG_PILOT_PA        0x10
#define MAX30102_REG_MULTI_LED_CTRL1 0x11
#define MAX30102_REG_MULTI_LED_CTRL2 0x12
#define MAX30102_REG_TEMP_INTR       0x1F
#define MAX30102_REG_TEMP_FRAC       0x20
#define MAX30102_REG_TEMP_CONFIG     0x21
#define MAX30102_REG_PROX_INT_THRESH 0x30
#define MAX30102_REG_REV_ID          0xFE
#define MAX30102_REG_PART_ID         0xFF

#define MAX30102_ADDR 0xAE

bool max30102_write_reg(max30102_t* self, u8 addr, u8 data)
{
    if (!g_port) {
        debug_print("[max30102] port not registered\n");
        return false;
    }
    i32 ret = g_port->i2c_write(self->hi2c, MAX30102_ADDR, addr, 1, &data, 1, 100);
    return ret == 0;
}

bool max30102_read_reg(max30102_t* self, u8 addr, u8* data)
{
    if (!g_port) {
        debug_print("[max30102] port not registered\n");
        return false;
    }
    i32 ret = g_port->i2c_read(self->hi2c, MAX30102_ADDR, addr, 1, data, 1, 100);
    return ret == 0;
}

bool max30102_init(max30102_t* self, void* hi2c)
{
    if (self == NULL) {
        return false;
    }
    self->hi2c = hi2c;
    
    if (!max30102_write_reg(self, MAX30102_REG_INTR_ENABLE_1, 0xc0)) return false;
    if (!max30102_write_reg(self, MAX30102_REG_INTR_ENABLE_2, 0x00)) return false;
    if (!max30102_write_reg(self, MAX30102_REG_FIFO_WR_PTR, 0x00)) return false;
    if (!max30102_write_reg(self, MAX30102_REG_OVF_COUNTER, 0x00)) return false;
    if (!max30102_write_reg(self, MAX30102_REG_FIFO_RD_PTR, 0x00)) return false;
    if (!max30102_write_reg(self, MAX30102_REG_FIFO_CONFIG, 0x0f)) return false;
    if (!max30102_write_reg(self, MAX30102_REG_MODE_CONFIG, 0x03)) return false;
    if (!max30102_write_reg(self, MAX30102_REG_SPO2_CONFIG, 0x27)) return false;
    if (!max30102_write_reg(self, MAX30102_REG_LED1_PA, 0x24)) return false;
    if (!max30102_write_reg(self, MAX30102_REG_LED2_PA, 0x24)) return false;
    if (!max30102_write_reg(self, MAX30102_REG_PILOT_PA, 0x7f)) return false;
    
    return true;
}

bool max30102_read_fifo(max30102_t* self, u32* red_led, u32* ir_led)
{
    u8 status;
    u8 data[6];
    
    if (self == NULL || red_led == NULL || ir_led == NULL) {
        return false;
    }

    // Read and clear status register
    if (!max30102_read_reg(self, MAX30102_REG_INTR_STATUS_1, &status)) return false;
    if (!max30102_read_reg(self, MAX30102_REG_INTR_STATUS_2, &status)) return false;
    
    if (!g_port) {
        debug_print("[max30102] port not registered\n");
        return false;
    }
    
    i32 ret = g_port->i2c_read(self->hi2c, MAX30102_ADDR, MAX30102_REG_FIFO_DATA, 1, data, 6, 100);
    if (ret != 0) return false;
    
    *red_led = ((u32)data[0] << 16) | ((u32)data[1] << 8) | (u32)data[2];
    *ir_led = ((u32)data[3] << 16) | ((u32)data[4] << 8) | (u32)data[5];
    
    *red_led &= 0x03FFFF;
    *ir_led &= 0x03FFFF;
    
    return true;
}

bool max30102_reset(max30102_t* self)
{
    if (self == NULL) {
        return false;
    }
    return max30102_write_reg(self, MAX30102_REG_MODE_CONFIG, 0x40);
}

