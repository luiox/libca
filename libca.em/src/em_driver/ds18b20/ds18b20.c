#include "ds18b20.h"

////////////////////////////////////////////////////////////////////////////////

#if (LIBCA_DS18B20_PORT_MODE == LIBCA_DS18B20_PORT_MODE_EXTERN)
#    define DS18B20_WRITE_PIN(gpio, pin, v) port_ds18b20_write_pin((gpio), (pin), (v))
#    define DS18B20_READ_PIN(gpio, pin) port_ds18b20_read_pin((gpio), (pin))
#    define DS18B20_SET_OUTPUT_MODE(gpio, pin) port_ds18b20_set_output_mode((gpio), (pin))
#    define DS18B20_SET_INPUT_MODE(gpio, pin) port_ds18b20_set_input_mode((gpio), (pin))
#    define DS18B20_DELAY_US(us) port_ds18b20_delay_us(us)

#elif (LIBCA_DS18B20_PORT_MODE == LIBCA_DS18B20_PORT_MODE_DYNAMIC)
static const ds18b20_port_t* g_ds18b20_port = NULL;
#    define DS18B20_WRITE_PIN(gpio, pin, v) g_ds18b20_port->write_pin((gpio), (pin), (v))
#    define DS18B20_READ_PIN(gpio, pin) g_ds18b20_port->read_pin((gpio), (pin))
#    define DS18B20_SET_OUTPUT_MODE(gpio, pin) g_ds18b20_port->set_output_mode((gpio), (pin))
#    define DS18B20_SET_INPUT_MODE(gpio, pin) g_ds18b20_port->set_input_mode((gpio), (pin))
#    define DS18B20_DELAY_US(us) g_ds18b20_port->delay_us(us)

#else
#    error "Invalid DS18B20 port mode"
#endif

#if (LIBCA_DS18B20_PORT_MODE == LIBCA_DS18B20_PORT_MODE_DYNAMIC)
void ds18b20_bind_port(const ds18b20_port_t* port)
{
    g_ds18b20_port = port;
}
bool ds18b20_port_is_registered(void)
{
    return g_ds18b20_port != NULL;
}
#endif

////////////////////////////////////////////////////////////////////////////////

// 简化访问宏
#define DS_OUT(self, n) DS18B20_WRITE_PIN((self)->gpio, (self)->pin, (n))
#define DS_IN(self) DS18B20_READ_PIN((self)->gpio, (self)->pin)
#define DS_OUTPUT_MODE(self) DS18B20_SET_OUTPUT_MODE((self)->gpio, (self)->pin)
#define DS_INPUT_MODE(self) DS18B20_SET_INPUT_MODE((self)->gpio, (self)->pin)

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
    DS18B20_DELAY_US(750);

    // 释放（拉高）15~60 us
    DS_OUT(self, 1);
    DS18B20_DELAY_US(15);
}

static i32 ds18b20_check_ready_single(ds18b20_t* self)
{
    uint16_t cnt = 0;

    // 等待存在脉冲（presence pulse，60~240 us）
    DS_INPUT_MODE(self);
    while (DS_IN(self) && cnt < 240) {
        DS18B20_DELAY_US(1);
        cnt++;
    }

    if (cnt >= 240) {
        return DS18B20_ERR_NO_PRESENCE;
    }

    // 等待释放脉冲（release pulse，60~240 us）
    cnt = 0;
    DS_INPUT_MODE(self);
    while ((!DS_IN(self)) && cnt < 240) {
        DS18B20_DELAY_US(1);
        cnt++;
    }

    if (cnt >= 240) {
        return DS18B20_ERR_NO_RELEASE;
    }

    return DS18B20_OK;
}

/// 检查设备是否存在
/// 返回：DS18B20_OK 表示存在，1/2 表示不同的超时错误（参见 ds18b20 内部实现），-1 表示 port 未注册
i32 ds18b20_check_device(ds18b20_t* self)
{
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
        DS18B20_DELAY_US(2);
        // 写入比特位
        DS_OUT(self, cmd & 0x01);
        DS18B20_DELAY_US(60);
        // 释放总线（拉高）
        DS_OUT(self, 1);
        cmd >>= 1;
        DS18B20_DELAY_US(2);
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
        DS18B20_DELAY_US(2);
        DS_OUT(self, 1);

        DS_INPUT_MODE(self);
        DS18B20_DELAY_US(10);

        data >>= 1;
        if (DS_IN(self)) {
            data |= 0x80;
        }

        DS18B20_DELAY_US(60);
        // 读时隙结束后把总线释放为高电平
        DS_OUT(self, 1);
    }

    return data;
}

/// 读取温度
/// 输出：*temp 为原始 16 位温度值，单位为 1/16 °C（即低 4 位为小数部分）
/// 返回：0 成功，负值为错误码
i32 ds18b20_read_temperature(ds18b20_t* self, u16* temp)
{
    u8  temp_L, temp_H;
    i32 ret;

    ret = ds18b20_check_device(self);
    if (ret != DS18B20_OK) {
        return ret;   // 返回更具体的错误码
    }

    ds18b20_write_byte(self, 0xCC);   // skip ROM
    ds18b20_write_byte(self, 0x44);   // convert T

    // 等待转换完成（设备忙时会返回 0xFF）
    while (ds18b20_read_byte(self) != 0xFF)
        ;

    ret = ds18b20_check_device(self);
    if (ret != DS18B20_OK) {
        return ret;   // 返回更具体的错误码
    }

    ds18b20_write_byte(self, 0xCC);   // skip ROM
    ds18b20_write_byte(self, 0xBE);   // read scratchpad

    temp_L = ds18b20_read_byte(self);
    temp_H = ds18b20_read_byte(self);

    *temp = (u16)temp_L | ((u16)temp_H << 8);

    return DS18B20_OK;
}
