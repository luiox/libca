#include "ds1302.h"
#include "../em_base/debug.h"

// 寄存器地址定义
#define DS1302_REG_SECOND       0x80
#define DS1302_REG_MINUTE       0x82
#define DS1302_REG_HOUR         0x84
#define DS1302_REG_DAY          0x86
#define DS1302_REG_MONTH        0x88
#define DS1302_REG_WEEK         0x8A
#define DS1302_REG_YEAR         0x8C
#define DS1302_REG_WRITE_PROTECT 0x8E

static const ds1302_port_t* g_port = NULL;

void ds1302_bind_port(const ds1302_port_t* port) {
    param_check(port != NULL);
    g_port = port;
}

bool ds1302_port_is_registered(void) {
    return g_port != NULL;
}

static u8 bcd_to_dec(u8 bcd) {
    return (bcd >> 4) * 10 + (bcd & 0x0F);
}

static u8 dec_to_bcd(u8 dec) {
    return ((dec / 10) << 4) | (dec % 10);
}

void ds1302_init(ds1302_t* self) {
    param_check(self != NULL);

    if (!ds1302_port_is_registered()) {
        debug_print("[ds1302] error: port not registered\n");
        return;
    }

    // 初始化时，确认为输出模式并拉低
    g_port->set_output_mode(self->ce_port, self->ce_pin);
    g_port->set_output_mode(self->sclk_port, self->sclk_pin);
    g_port->set_output_mode(self->data_port, self->data_pin);

    g_port->write_pin(self->ce_port, self->ce_pin, 0);
    g_port->write_pin(self->sclk_port, self->sclk_pin, 0);
    
    // 关闭写保护
    ds1302_write_reg(self, DS1302_REG_WRITE_PROTECT, 0x00);
}

void ds1302_write_byte(ds1302_t* self, u8 data) {
    param_check(self != NULL);

    g_port->set_output_mode(self->data_port, self->data_pin);
    for (u8 i = 0; i < 8; i++) {
        g_port->write_pin(self->sclk_port, self->sclk_pin, 0);
        g_port->write_pin(self->data_port, self->data_pin, (data >> i) & 0x01);
        g_port->delay_us(1);
        g_port->write_pin(self->sclk_port, self->sclk_pin, 1);
        g_port->delay_us(1);
    }
}

void ds1302_write_reg(ds1302_t* self, u8 address, u8 data) {
    param_check(self != NULL);

    g_port->write_pin(self->ce_port, self->ce_pin, 0);
    g_port->write_pin(self->sclk_port, self->sclk_pin, 0);
    g_port->delay_us(1);
    g_port->write_pin(self->ce_port, self->ce_pin, 1);
    g_port->delay_us(1);

    ds1302_write_byte(self, address & ~0x01); // 确保第0位为0表示写
    ds1302_write_byte(self, data);

    g_port->write_pin(self->ce_port, self->ce_pin, 0);
    g_port->write_pin(self->sclk_port, self->sclk_pin, 0);
}

u8 ds1302_read_reg(ds1302_t* self, u8 address) {
    param_check(self != NULL);
    u8 data = 0;

    g_port->write_pin(self->ce_port, self->ce_pin, 0);
    g_port->write_pin(self->sclk_port, self->sclk_pin, 0);
    g_port->delay_us(1);
    g_port->write_pin(self->ce_port, self->ce_pin, 1);
    g_port->delay_us(1);

    ds1302_write_byte(self, address | 0x01); // 确保第0位为1表示读
    
    g_port->set_input_mode(self->data_port, self->data_pin);
    for (u8 i = 0; i < 8; i++) {
        g_port->write_pin(self->sclk_port, self->sclk_pin, 0);
        g_port->delay_us(1);
        g_port->write_pin(self->sclk_port, self->sclk_pin, 1);
        g_port->delay_us(1);
        if (g_port->read_pin(self->data_port, self->data_pin)) {
            data |= (1 << i);
        }
    }

    g_port->write_pin(self->ce_port, self->ce_pin, 0);
    g_port->write_pin(self->sclk_port, self->sclk_pin, 0);

    return data;
}

void ds1302_get_time(ds1302_t* self, ds1302_time_t* time) {
    param_check(self != NULL);
    param_check(time != NULL);

    time->second = bcd_to_dec(ds1302_read_reg(self, DS1302_REG_SECOND) & 0x7F);
    time->minute = bcd_to_dec(ds1302_read_reg(self, DS1302_REG_MINUTE) & 0x7F);
    time->hour = bcd_to_dec(ds1302_read_reg(self, DS1302_REG_HOUR) & 0x3F);
    time->day = bcd_to_dec(ds1302_read_reg(self, DS1302_REG_DAY) & 0x3F);
    time->month = bcd_to_dec(ds1302_read_reg(self, DS1302_REG_MONTH) & 0x1F);
    time->week = bcd_to_dec(ds1302_read_reg(self, DS1302_REG_WEEK) & 0x07);
    time->year = bcd_to_dec(ds1302_read_reg(self, DS1302_REG_YEAR)) + 2000;
}

void ds1302_set_time(ds1302_t* self, const ds1302_time_t* time) {
    param_check(self != NULL);
    param_check(time != NULL);

    ds1302_write_reg(self, DS1302_REG_WRITE_PROTECT, 0x00); // 关闭写保护
    ds1302_write_reg(self, DS1302_REG_SECOND, dec_to_bcd(time->second));
    ds1302_write_reg(self, DS1302_REG_MINUTE, dec_to_bcd(time->minute));
    ds1302_write_reg(self, DS1302_REG_HOUR, dec_to_bcd(time->hour));
    ds1302_write_reg(self, DS1302_REG_DAY, dec_to_bcd(time->day));
    ds1302_write_reg(self, DS1302_REG_MONTH, dec_to_bcd(time->month));
    ds1302_write_reg(self, DS1302_REG_WEEK, dec_to_bcd(time->week));
    ds1302_write_reg(self, DS1302_REG_YEAR, dec_to_bcd(time->year % 100));
    ds1302_write_reg(self, DS1302_REG_WRITE_PROTECT, 0x80); // 开启写保护
}
