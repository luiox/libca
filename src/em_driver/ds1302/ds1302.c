#include "ds1302.h"
#include <em_base/debug.h>

////////////////////////////////////////////////////////////////////////////////

#if (LIBCA_DS1302_PORT_MODE == LIBCA_DS1302_PORT_MODE_EXTERN)
#define DS1302_WRITE_PIN(gpio, pin, v)    port_ds1302_write_pin((gpio), (pin), (v))
#define DS1302_READ_PIN(gpio, pin)        port_ds1302_read_pin((gpio), (pin))
#define DS1302_SET_OUTPUT_MODE(gpio, pin) port_ds1302_set_output_mode((gpio), (pin))
#define DS1302_SET_INPUT_MODE(gpio, pin)  port_ds1302_set_input_mode((gpio), (pin))
#define DS1302_DELAY_US(us)               port_ds1302_delay_us(us)

#elif (LIBCA_DS1302_PORT_MODE == LIBCA_DS1302_PORT_MODE_DYNAMIC)
static const ds1302_port_t* g_ds1302_port = NULL;
#define DS1302_WRITE_PIN(gpio, pin, v)    g_ds1302_port->write_pin((gpio), (pin), (v))
#define DS1302_READ_PIN(gpio, pin)        g_ds1302_port->read_pin((gpio), (pin))
#define DS1302_SET_OUTPUT_MODE(gpio, pin) g_ds1302_port->set_output_mode((gpio), (pin))
#define DS1302_SET_INPUT_MODE(gpio, pin)  g_ds1302_port->set_input_mode((gpio), (pin))
#define DS1302_DELAY_US(us)               g_ds1302_port->delay_us(us)

#else
#error "Invalid DS1302 port mode"
#endif

#if (LIBCA_DS1302_PORT_MODE == LIBCA_DS1302_PORT_MODE_DYNAMIC)
void ds1302_bind_port(const ds1302_port_t* port) { g_ds1302_port = port; }
bool ds1302_port_is_registered(void) { return g_ds1302_port != NULL; }
#endif

////////////////////////////////////////////////////////////////////////////////

// 寄存器地址定义
#define DS1302_REG_SECOND       0x80
#define DS1302_REG_MINUTE       0x82
#define DS1302_REG_HOUR         0x84
#define DS1302_REG_DAY          0x86
#define DS1302_REG_MONTH        0x88
#define DS1302_REG_WEEK         0x8A
#define DS1302_REG_YEAR         0x8C
#define DS1302_REG_WRITE_PROTECT 0x8E

// 突发模式命令
#define DS1302_CMD_BURST_READ   0xBF
#define DS1302_CMD_BURST_WRITE  0xBE

// 寄存器掩码
#define DS1302_MASK_SECOND      0x7F
#define DS1302_MASK_MINUTE      0x7F
#define DS1302_MASK_HOUR        0x3F
#define DS1302_MASK_DAY         0x3F
#define DS1302_MASK_MONTH       0x1F
#define DS1302_MASK_WEEK        0x07
#define DS1302_MASK_YEAR        0xFF

/**
 * @brief BCD码转十进制数
 * @param bcd BCD格式数据 (0x00-0x99)
 * @return 十进制数值 (0-99)
 */
static inline u8 bcd_to_dec(u8 bcd) {
    return (bcd >> 4) * 10 + (bcd & 0x0F);
}

/**
 * @brief 十进制数转BCD码
 * 请确保dec不超过99，以免出错
 * @param dec 十进制数值 (0-99)
 * @return BCD格式数据 (0x00-0x99)
 */
static inline u8 dec_to_bcd(u8 dec) {
    // 利用恒等式：x = (x/10)*10 + (x%10)
    // 这里 (dec / 10) 直接作为高 nibble
    // (dec % 10) 直接作为低 nibble
    return ((dec / 10) << 4) | (dec % 10);
}

void ds1302_init(ds1302_t* self) {
    param_check(self != NULL);

    // 初始化时，确认为输出模式并拉低
    DS1302_SET_OUTPUT_MODE(self->ce_port, self->ce_pin);
    DS1302_SET_OUTPUT_MODE(self->sclk_port, self->sclk_pin);
    DS1302_SET_OUTPUT_MODE(self->data_port, self->data_pin);

    DS1302_WRITE_PIN(self->ce_port, self->ce_pin, 0);
    DS1302_WRITE_PIN(self->sclk_port, self->sclk_pin, 0);
    
    // 关闭写保护
    ds1302_write_reg(self, DS1302_REG_WRITE_PROTECT, 0x00);
}

void ds1302_write_byte(ds1302_t* self, u8 data) {
    param_check(self != NULL);

    DS1302_SET_OUTPUT_MODE(self->data_port, self->data_pin);
    for (u8 i = 0; i < 8; i++) {
        DS1302_WRITE_PIN(self->sclk_port, self->sclk_pin, 0);
        DS1302_WRITE_PIN(self->data_port, self->data_pin, (data >> i) & 0x01);
        DS1302_DELAY_US(1);
        DS1302_WRITE_PIN(self->sclk_port, self->sclk_pin, 1);
        DS1302_DELAY_US(1);
    }
}

/**
 * @brief 从DS1302读取一字节数据 (私有辅助函数)
 * 
 * @param self 驱动对象
 * @return u8 读取到的字节
 */
static u8 ds1302_read_byte(ds1302_t* self) {
    param_check(self != NULL);
    u8 data = 0;
    DS1302_SET_INPUT_MODE(self->data_port, self->data_pin);
    for (u8 i = 0; i < 8; i++) {
        DS1302_WRITE_PIN(self->sclk_port, self->sclk_pin, 0);
        DS1302_DELAY_US(1);
        DS1302_WRITE_PIN(self->sclk_port, self->sclk_pin, 1);
        DS1302_DELAY_US(1);
        if (DS1302_READ_PIN(self->data_port, self->data_pin)) {
            data |= (1 << i);
        }
    }
    return data;
}

void ds1302_write_reg(ds1302_t* self, u8 address, u8 data) {
    param_check(self != NULL);

    DS1302_WRITE_PIN(self->ce_port, self->ce_pin, 0);
    DS1302_WRITE_PIN(self->sclk_port, self->sclk_pin, 0);
    DS1302_DELAY_US(1);
    DS1302_WRITE_PIN(self->ce_port, self->ce_pin, 1);
    DS1302_DELAY_US(1);

    ds1302_write_byte(self, address); // 确保第0位为0表示写
    ds1302_write_byte(self, data);

    DS1302_WRITE_PIN(self->ce_port, self->ce_pin, 0);
    DS1302_WRITE_PIN(self->sclk_port, self->sclk_pin, 0);
}

u8 ds1302_read_reg(ds1302_t* self, u8 address) {
    param_check(self != NULL);
    u8 data = 0;

    DS1302_WRITE_PIN(self->ce_port, self->ce_pin, 0);
    DS1302_WRITE_PIN(self->sclk_port, self->sclk_pin, 0);
    DS1302_DELAY_US(1);
    DS1302_WRITE_PIN(self->ce_port, self->ce_pin, 1);
    DS1302_DELAY_US(1);

    ds1302_write_byte(self, address | 0x01); // 确保第0位为1表示读
    data = ds1302_read_byte(self);

    DS1302_WRITE_PIN(self->ce_port, self->ce_pin, 0);
    DS1302_WRITE_PIN(self->sclk_port, self->sclk_pin, 0);

    return data;
}

void ds1302_get_time(ds1302_t* self, ds1302_time_t* time) {
    param_check(self != NULL);
    param_check(time != NULL);

    time->second = bcd_to_dec(ds1302_read_reg(self, DS1302_REG_SECOND) & DS1302_MASK_SECOND);
    time->minute = bcd_to_dec(ds1302_read_reg(self, DS1302_REG_MINUTE) & DS1302_MASK_MINUTE);
    time->hour = bcd_to_dec(ds1302_read_reg(self, DS1302_REG_HOUR) & DS1302_MASK_HOUR);
    time->day = bcd_to_dec(ds1302_read_reg(self, DS1302_REG_DAY) & DS1302_MASK_DAY);
    time->month = bcd_to_dec(ds1302_read_reg(self, DS1302_REG_MONTH) & DS1302_MASK_MONTH);
    time->week = bcd_to_dec(ds1302_read_reg(self, DS1302_REG_WEEK) & DS1302_MASK_WEEK);
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
    ds1302_write_reg(self, DS1302_REG_YEAR, dec_to_bcd((u8)(time->year % 100)));
    ds1302_write_reg(self, DS1302_REG_WRITE_PROTECT, 0x80); // 开启写保护
}

void ds1302_get_time_fast(ds1302_t* self, ds1302_time_t* time) {
    param_check(self != NULL);
    param_check(time != NULL);

    DS1302_WRITE_PIN(self->ce_port, self->ce_pin, 0);
    DS1302_WRITE_PIN(self->sclk_port, self->sclk_pin, 0);
    DS1302_DELAY_US(1);
    DS1302_WRITE_PIN(self->ce_port, self->ce_pin, 1);
    DS1302_DELAY_US(1);

    ds1302_write_byte(self, DS1302_CMD_BURST_READ);

    time->second = bcd_to_dec(ds1302_read_byte(self) & DS1302_MASK_SECOND);
    time->minute = bcd_to_dec(ds1302_read_byte(self) & DS1302_MASK_MINUTE);
    time->hour = bcd_to_dec(ds1302_read_byte(self) & DS1302_MASK_HOUR);
    time->day = bcd_to_dec(ds1302_read_byte(self) & DS1302_MASK_DAY);
    time->month = bcd_to_dec(ds1302_read_byte(self) & DS1302_MASK_MONTH);
    time->week = bcd_to_dec(ds1302_read_byte(self) & DS1302_MASK_WEEK);
    time->year = bcd_to_dec(ds1302_read_byte(self)) + 2000;
    
    // 突发模式读8个字节，最后一个是控制寄存器，忽略
    (void)ds1302_read_byte(self);

    DS1302_WRITE_PIN(self->ce_port, self->ce_pin, 0);
    DS1302_WRITE_PIN(self->sclk_port, self->sclk_pin, 0);
}

void ds1302_set_time_fast(ds1302_t* self, const ds1302_time_t* time) {
    param_check(self != NULL);
    param_check(time != NULL);

    ds1302_write_reg(self, DS1302_REG_WRITE_PROTECT, 0x00); // 关闭写保护

    DS1302_WRITE_PIN(self->ce_port, self->ce_pin, 0);
    DS1302_WRITE_PIN(self->sclk_port, self->sclk_pin, 0);
    DS1302_DELAY_US(1);
    DS1302_WRITE_PIN(self->ce_port, self->ce_pin, 1);
    DS1302_DELAY_US(1);

    ds1302_write_byte(self, DS1302_CMD_BURST_WRITE);

    ds1302_write_byte(self, dec_to_bcd(time->second));
    ds1302_write_byte(self, dec_to_bcd(time->minute));
    ds1302_write_byte(self, dec_to_bcd(time->hour));
    ds1302_write_byte(self, dec_to_bcd(time->day));
    ds1302_write_byte(self, dec_to_bcd(time->month));
    ds1302_write_byte(self, dec_to_bcd(time->week));
    ds1302_write_byte(self, dec_to_bcd((u8)(time->year % 100)));
    ds1302_write_byte(self, 0x00); // 控制寄存器

    DS1302_WRITE_PIN(self->ce_port, self->ce_pin, 0);
    DS1302_WRITE_PIN(self->sclk_port, self->sclk_pin, 0);

    ds1302_write_reg(self, DS1302_REG_WRITE_PROTECT, 0x80); // 开启写保护
}
