#include "soft_i2c.h"
#include <em_base/debug.h>

static const soft_i2c_port_t* g_port = NULL;

void soft_i2c_bind_port(const soft_i2c_port_t* port)
{
    param_check(port != NULL);
    g_port = port;
}

bool soft_i2c_port_is_registered(void)
{
    return g_port != NULL;
}

// 内部便捷宏，要求已绑定 port
#define I2C_SDA_OUT(self) g_port->gpio_set_output_mode((self)->sda_port, (self)->sda_pin)
#define I2C_SDA_IN(self) g_port->gpio_set_input_mode((self)->sda_port, (self)->sda_pin)
#define I2C_SDA_H(self) g_port->gpio_write((self)->sda_port, (self)->sda_pin, 1)
#define I2C_SDA_L(self) g_port->gpio_write((self)->sda_port, (self)->sda_pin, 0)
#define I2C_SCL_H(self) g_port->gpio_write((self)->scl_port, (self)->scl_pin, 1)
#define I2C_SCL_L(self) g_port->gpio_write((self)->scl_port, (self)->scl_pin, 0)
#define I2C_SDA_READ(self) g_port->gpio_read((self)->sda_port, (self)->sda_pin)
#define I2C_DELAY() g_port->delay_us(2)

void soft_i2c_init(soft_i2c_t* self)
{
    param_check(self != NULL);
    if (!soft_i2c_port_is_registered())
        return;

    g_port->gpio_set_output_mode(self->scl_port, self->scl_pin);
    g_port->gpio_set_output_mode(self->sda_port, self->sda_pin);

    soft_i2c_stop(self);
}

void soft_i2c_start(soft_i2c_t* self)
{
    param_check(self != NULL);
    if (!soft_i2c_port_is_registered())
        return;

    I2C_SDA_OUT(self);
    I2C_SDA_H(self);
    I2C_SCL_H(self);
    I2C_DELAY();
    I2C_SDA_L(self);
    I2C_DELAY();
    I2C_SCL_L(self);
    I2C_DELAY();
}

void soft_i2c_stop(soft_i2c_t* self)
{
    param_check(self != NULL);
    if (!soft_i2c_port_is_registered())
        return;

    I2C_SDA_OUT(self);
    I2C_SDA_L(self);
    I2C_SCL_H(self);
    I2C_DELAY();
    I2C_SDA_H(self);
    I2C_DELAY();
}

u8 soft_i2c_wait_ack(soft_i2c_t* self)
{
    param_check(self != NULL);
    if (!soft_i2c_port_is_registered())
        return 1;

    u8 timeout = 0;
    I2C_SDA_H(self);
    I2C_SDA_IN(self);
    I2C_DELAY();
    I2C_SCL_H(self);
    I2C_DELAY();
    while (I2C_SDA_READ(self)) {
        timeout++;
        if (timeout > 100) {
            soft_i2c_stop(self);
            return 1;
        }
    }
    I2C_SCL_L(self);
    I2C_DELAY();
    return 0;
}

void soft_i2c_ack(soft_i2c_t* self)
{
    param_check(self != NULL);
    if (!soft_i2c_port_is_registered())
        return;

    I2C_SCL_L(self);
    I2C_SDA_OUT(self);
    I2C_SDA_L(self);
    I2C_DELAY();
    I2C_SCL_H(self);
    I2C_DELAY();
    I2C_SCL_L(self);
    I2C_DELAY();
    I2C_SDA_H(self);
}

void soft_i2c_nack(soft_i2c_t* self)
{
    param_check(self != NULL);
    if (!soft_i2c_port_is_registered())
        return;

    I2C_SCL_L(self);
    I2C_SDA_OUT(self);
    I2C_SDA_H(self);
    I2C_DELAY();
    I2C_SCL_H(self);
    I2C_DELAY();
    I2C_SCL_L(self);
    I2C_DELAY();
}

u8 soft_i2c_send_byte(soft_i2c_t* self, u8 byte)
{
    param_check(self != NULL);
    if (!soft_i2c_port_is_registered())
        return 1;

    I2C_SDA_OUT(self);
    // I2C_SCL_L(self); // 已经在上一位操作结尾拉低了
    for (u8 i = 0; i < 8; i++) {
        if (byte & 0x80) {
            I2C_SDA_H(self);
        }
        else {
            I2C_SDA_L(self);
        }
        byte <<= 1;
        I2C_DELAY();
        I2C_SCL_H(self);
        I2C_DELAY();
        I2C_SCL_L(self);
        I2C_DELAY();
    }
    return soft_i2c_wait_ack(self);
}

u8 soft_i2c_read_byte(soft_i2c_t* self, u8 ack)
{
    param_check(self != NULL);
    if (!soft_i2c_port_is_registered())
        return 0;

    u8 data = 0;
    I2C_SDA_IN(self);
    for (u8 i = 0; i < 8; i++) {
        data <<= 1;
        I2C_SCL_H(self);
        I2C_DELAY();
        if (I2C_SDA_READ(self)) {
            data++;
        }
        I2C_SCL_L(self);
        I2C_DELAY();
    }

    if (ack) {
        soft_i2c_ack(self);
    }
    else {
        soft_i2c_nack(self);
    }

    return data;
}

u8 soft_i2c_check_device(soft_i2c_t* self, u8 address)
{
    param_check(self != NULL);
    if (!soft_i2c_port_is_registered())
        return 1;

    soft_i2c_start(self);
    u8 ack = soft_i2c_send_byte(self, address | I2C_WRITE);
    soft_i2c_stop(self);
    return ack;
}

// 高层接口实现

i32 soft_i2c_master_write(soft_i2c_t* self, u16 dev_addr, u8* data, u16 data_size, u32 timeout)
{
    param_check(self != NULL);
    param_check(data != NULL);
    unused_param(timeout);

    soft_i2c_start(self);
    if (soft_i2c_send_byte(self, (u8)dev_addr | I2C_WRITE)) {
        soft_i2c_stop(self);
        return -1;
    }

    for (u16 i = 0; i < data_size; i++) {
        if (soft_i2c_send_byte(self, data[i])) {
            soft_i2c_stop(self);
            return -1;
        }
    }

    soft_i2c_stop(self);
    return 0;
}

i32 soft_i2c_master_read(soft_i2c_t* self, u16 dev_addr, u8* data, u16 data_size, u32 timeout)
{
    param_check(self != NULL);
    param_check(data != NULL);
    unused_param(timeout);

    soft_i2c_start(self);
    if (soft_i2c_send_byte(self, (u8)dev_addr | I2C_READ)) {
        soft_i2c_stop(self);
        return -1;
    }

    for (u16 i = 0; i < data_size; i++) {
        data[i] = soft_i2c_read_byte(self, (i == data_size - 1) ? 0 : 1);
    }

    soft_i2c_stop(self);
    return 0;
}

i32 soft_i2c_mem_write(soft_i2c_t* self, u16 dev_addr, u16 mem_addr, u16 mem_addr_size, u8* data,
                       u16 data_size, u32 timeout)
{
    param_check(self != NULL);
    param_check(data != NULL);
    unused_param(timeout);

    soft_i2c_start(self);
    if (soft_i2c_send_byte(self, (u8)dev_addr | I2C_WRITE)) {
        soft_i2c_stop(self);
        return -1;
    }

    if (mem_addr_size == 2) {
        if (soft_i2c_send_byte(self, (u8)(mem_addr >> 8)) != 0) {
            // 发送失败，断开连接
            soft_i2c_stop(self);
            return -1;
        }
    }
    if (soft_i2c_send_byte(self, (u8)mem_addr) != 0) {
        soft_i2c_stop(self);
        return -1;
    }

    for (u16 i = 0; i < data_size; i++) {
        if (soft_i2c_send_byte(self, data[i]) != 0) {
            soft_i2c_stop(self);
            return -1;
        }
    }

    soft_i2c_stop(self);
    return 0;
}

i32 soft_i2c_mem_read(soft_i2c_t* self, u16 dev_addr, u16 mem_addr, u16 mem_addr_size, u8* data,
                      u16 data_size, u32 timeout)
{
    param_check(self != NULL);
    param_check(data != NULL);
    unused_param(timeout);

    soft_i2c_start(self);
    if (soft_i2c_send_byte(self, (u8)dev_addr | I2C_WRITE) != 0) {
        soft_i2c_stop(self);
        return -1;
    }

    if (mem_addr_size == 2) {
        if (soft_i2c_send_byte(self, (u8)(mem_addr >> 8)) != 0) {
            soft_i2c_stop(self);
            return -1;
        }
    }
    if (soft_i2c_send_byte(self, (u8)mem_addr) != 0) {
        soft_i2c_stop(self);
        return -1;
    }

    soft_i2c_start(self);   // 重启信号
    if (soft_i2c_send_byte(self, (u8)dev_addr | I2C_READ) != 0) {
        soft_i2c_stop(self);
        return -1;
    }

    for (u16 i = 0; i < data_size; i++) {
        data[i] = soft_i2c_read_byte(self, (i == data_size - 1) ? 0 : 1);
    }

    soft_i2c_stop(self);
    return 0;
}

i32 soft_i2c_is_device_ready(soft_i2c_t* self, u16 dev_addr, u32 trials, u32 timeout)
{
    param_check(self != NULL);
    unused_param(timeout);

    for (u32 i = 0; i < trials; i++) {
        if (soft_i2c_check_device(self, (u8)dev_addr) == 0) {
            return 0;
        }
    }
    return -1;
}

// 暂不支持从机模式的简单软件实现
i32 soft_i2c_slave_write(soft_i2c_t* self, u16 dev_addr, u8* data, u16 data_size, u32 timeout)
{
    unused_param(self);
    unused_param(dev_addr);
    unused_param(data);
    unused_param(data_size);
    unused_param(timeout);
    return -1;
}
i32 soft_i2c_slave_read(soft_i2c_t* self, u16 dev_addr, u8* data, u16 data_size, u32 timeout)
{
    unused_param(self);
    unused_param(dev_addr);
    unused_param(data);
    unused_param(data_size);
    unused_param(timeout);
    return -1;
}
