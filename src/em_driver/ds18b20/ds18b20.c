#include "ds18b20.h"

#if (LIBCA_DS18B20_PORT_MODE == LIBCA_DS18B20_PORT_MODE_EXTERN)
static const ds18b20_port_t g_ds18b20_port_extern_impl = {
    .write_pin = port_ds18b20_write_pin,
    .read_pin = port_ds18b20_read_pin,
    .set_output_mode = port_ds18b20_set_output_mode,
    .set_input_mode = port_ds18b20_set_input_mode,
    .delay_us = port_ds18b20_delay_us,
};
static const ds18b20_port_t* g_ds18b20_port = &g_ds18b20_port_extern_impl;
#elif (LIBCA_DS18B20_PORT_MODE == LIBCA_DS18B20_PORT_MODE_DYNAMIC)
static const ds18b20_port_t* g_ds18b20_port = NULL;
#else
#error "Invalid DS18B20 port mode"
#endif

#ifndef delay_us
#define delay_us(us) g_ds18b20_port->delay_us(us)
#endif

void ds18b20_bind_port(const ds18b20_port_t* port)
{
    g_ds18b20_port = port;
}

bool ds18b20_port_is_registered(void)
{
    return g_ds18b20_port != NULL;
}

// 简化访问宏
#define DS_OUT(self, n)       g_ds18b20_port->write_pin((self)->gpio, (self)->pin, (n))
#define DS_IN(self)           g_ds18b20_port->read_pin((self)->gpio, (self)->pin)
#define DS_OUTPUT_MODE(self)  g_ds18b20_port->set_output_mode((self)->gpio, (self)->pin)
#define DS_INPUT_MODE(self)   g_ds18b20_port->set_input_mode((self)->gpio, (self)->pin)

void ds18b20_init(ds18b20_t* self, void* gpio, u16 pin)
{
    self->gpio = gpio;
    self->pin  = pin;
}

static void ds18b20_send_reset_single(ds18b20_t* self)
{
    DS_OUTPUT_MODE(self);

    // 复位脉冲 480~960 us
    DS_OUT(self, 0);
    delay_us(750);

    // 释放（拉高）15~60 us
    DS_OUT(self, 1);
    delay_us(15);
}

static i32 ds18b20_check_ready_single(ds18b20_t* self)
{
    uint16_t cnt = 0;

    // 等待存在脉冲（presence pulse，60~240 us）
    DS_INPUT_MODE(self);
    while (DS_IN(self) && cnt < 240) {
        delay_us(1);
        cnt++;
    }

    if (cnt >= 240) {
        return DS18B20_ERR_NO_PRESENCE;
    }

    // 等待释放脉冲（release pulse，60~240 us）
    cnt = 0;
    DS_INPUT_MODE(self);
    while ((!DS_IN(self)) && cnt < 240) {
        delay_us(1);
        cnt++;
    }

    if (cnt >= 240) {
        return DS18B20_ERR_NO_RELEASE;
    }

    return DS18B20_OK;
}

/**
 * 检查设备是否存在
 * 返回：DS18B20_OK 表示存在，1/2 表示不同的超时错误（参见 ds18b20 内部实现），-1 表示 port 未注册
 */
i32 ds18b20_check_device(ds18b20_t* self)
{
    if (!g_ds18b20_port) {
        return DS18B20_ERR_PORT_NOT_REGISTERED;
    }

    ds18b20_send_reset_single(self);
    return ds18b20_check_ready_single(self);
}

static uint8_t ds18b20_write_byte(ds18b20_t* self, uint8_t cmd)
{
    uint8_t i;

    for (i = 0; i < 8; i++) {
        DS_OUTPUT_MODE(self);
        // 起始时隙
        DS_OUT(self, 0);
        delay_us(2);
        // 写入比特位
        DS_OUT(self, cmd & 0x01);
        delay_us(60);
        // 释放总线（拉高）
        DS_OUT(self, 1);
        cmd >>= 1;
        delay_us(2);
    }

    return 0;
}

static uint8_t ds18b20_read_byte(ds18b20_t* self)
{
    uint8_t i, data = 0;

    for (i = 0; i < 8; i++) {
        DS_OUTPUT_MODE(self);
        // 发起读时隙
        DS_OUT(self, 0);
        delay_us(2);
        DS_OUT(self, 1);

        DS_INPUT_MODE(self);
        delay_us(10);

        data >>= 1;
        if (DS_IN(self)) {
            data |= 0x80;
        }

        delay_us(60);
        // 读时隙结束后把总线释放为高电平
        DS_OUT(self, 1);
    }

    return data;
}

/**
 * 读取温度
 * 输出：*temp 为原始 16 位温度值，单位为 1/16 °C（即低 4 位为小数部分）
 * 返回：0 成功，负值为错误码
 */
i32 ds18b20_read_temperature(ds18b20_t* self, u16* temp)
{
    u8 temp_L, temp_H;
    i32 ret;

    if (!g_ds18b20_port) {
        return DS18B20_ERR_PORT_NOT_REGISTERED;
    }

    ret = ds18b20_check_device(self);
    if (ret != DS18B20_OK) {
        return ret; // 返回更具体的错误码
    }

    ds18b20_write_byte(self, 0xCC); // skip ROM
    ds18b20_write_byte(self, 0x44); // convert T

    // 等待转换完成（设备忙时会返回 0xFF）
    while (ds18b20_read_byte(self) != 0xFF)
        ;

    ret = ds18b20_check_device(self);
    if (ret != DS18B20_OK) {
        return ret; // 返回更具体的错误码
    }

    ds18b20_write_byte(self, 0xCC); // skip ROM
    ds18b20_write_byte(self, 0xBE); // read scratchpad

    temp_L = ds18b20_read_byte(self);
    temp_H = ds18b20_read_byte(self);

    *temp = (u16)temp_L | ((u16)temp_H << 8);

    return DS18B20_OK;
}
