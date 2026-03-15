/**
 * @file simple_digital_sensor_template.h
 * @author canrad (1517807724@qq.com)
 * @brief 简单读取io的驱动模板生成宏工具
 * @version 0.1
 * @date 2026-01-23
 * 
 * @copyright Copyright (c) 2026
 * 
 */
#ifndef LIBCA_EM_DRIVER_SIMPLE_DIGITAL_SENSOR_TEMPLATE_H
#define LIBCA_EM_DRIVER_SIMPLE_DIGITAL_SENSOR_TEMPLATE_H

#include "em_base/datatype.h"
#include "em_base/macro_util.h"

#define SIMPLE_DIGITAL_SENSOR_TEMPLATE_DEFINE_HEADER(sensor_name, get_value_func_name)        \
    typedef struct CA_CONNECT2(sensor_name, _port)                                               \
    {                                                                                         \
        u8 (*read_pin)(void* gpio, u16 pin);                                                  \
    } CA_CONNECT2(sensor_name, _port_t);                                                         \
    void CA_CONNECT2(sensor_name, _bind_port)(const CA_CONNECT2(sensor_name, _port_t) * port);      \
    bool CA_CONNECT2(sensor_name, _port_is_registered)(void);                                    \
    typedef struct sensor_name                                                                \
    {                                                                                         \
        void* gpio;                                                                           \
        u16   pin;                                                                            \
    } CA_CONNECT2(sensor_name, _t);                                                              \
    void CA_CONNECT2(sensor_name, _init)(CA_CONNECT2(sensor_name, _t) * self, void* gpio, u16 pin); \
    u8   CA_CONNECT3(sensor_name, _, get_value_func_name)(CA_CONNECT2(sensor_name, _t) * self);

#define SIMPLE_DIGITAL_SENSOR_TEMPLATE_DEFINE_SOURCE(sensor_name, get_value_func_name)       \
    static const CA_CONNECT2(sensor_name, _port_t) * CA_CONNECT3(g_, sensor_name, _port) = NULL;   \
    void CA_CONNECT2(sensor_name, _bind_port)(const CA_CONNECT2(sensor_name, _port_t) * port)      \
    {                                                                                        \
        CA_CONNECT3(g_, sensor_name, _port) = port;                                             \
    }                                                                                        \
    bool CA_CONNECT2(sensor_name, _port_is_registered)(void)                                    \
    {                                                                                        \
        return CA_CONNECT3(g_, sensor_name, _port) != NULL;                                     \
    }                                                                                        \
    void CA_CONNECT2(sensor_name, _init)(CA_CONNECT2(sensor_name, _t) * self, void* gpio, u16 pin) \
    {                                                                                        \
        self->gpio = gpio;                                                                   \
        self->pin  = pin;                                                                    \
    }                                                                                        \
    u8 CA_CONNECT3(sensor_name, _, get_value_func_name)(CA_CONNECT2(sensor_name, _t) * self)       \
    {                                                                                        \
        return CA_CONNECT3(g_, sensor_name, _port)->read_pin(self->gpio, self->pin);            \
    }

#endif   // !LIBCA_EM_DRIVER_SIMPLE_DIGITAL_SENSOR_TEMPLATE_H
