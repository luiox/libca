#ifndef LIBCA_EM_DRIVER_SIMPLE_DIGITAL_SENSOR_TEMPLATE_H
#define LIBCA_EM_DRIVER_SIMPLE_DIGITAL_SENSOR_TEMPLATE_H

#include "../em_base/datatype.h"
#include "../em_base/macro_util.h"

#define SIMPLE_DIGITAL_SENSOR_TEMPLATE_DEFINE_HEADER(sensor_name, get_value_func_name)        \
    typedef struct CONNECT2(sensor_name, _port)                                               \
    {                                                                                         \
        u8   (*read_pin)(void* gpio, u16 pin);                                   \
    } CONNECT2(sensor_name, _port_t);                                                         \
    void CONNECT2(sensor_name, _bind_port)(const CONNECT2(sensor_name, _port_t) * port);      \
    bool CONNECT2(sensor_name, _port_is_registered)(void);                                    \
                                                                                              \
    typedef struct sensor_name                                                                \
    {                                                                                         \
        void* gpio;                                                                           \
        u16   pin;                                                                            \
    } CONNECT2(sensor_name, _t);                                                              \
                                                                                              \
    void CONNECT2(sensor_name, _init)(CONNECT2(sensor_name, _t) * self, void* gpio, u16 pin); \
    u8 CONNECT3(sensor_name, _, get_value_func_name)(CONNECT2(sensor_name, _t) * self);

#define SIMPLE_DIGITAL_SENSOR_TEMPLATE_DEFINE_SOURCE(sensor_name, get_value_func_name) \
\
static const CONNECT2(sensor_name, _port_t) * CONNECT3(g_, sensor_name, _port) = NULL;\
\
void CONNECT2(sensor_name, _bind_port)(const CONNECT2(sensor_name, _port_t) * port)\
{\
    CONNECT3(g_, sensor_name, _port) = port;\
}\
\
bool CONNECT2(sensor_name, _port_is_registered)(void)\
{\
    return CONNECT3(g_, sensor_name, _port) != NULL;\
}\
\
void CONNECT2(sensor_name, _init)(CONNECT2(sensor_name, _t) * self, void* gpio, u16 pin)\
{\
    self->gpio = gpio;\
    self->pin  = pin;\
}\
\
u8 CONNECT3(sensor_name, _, get_value_func_name)(CONNECT2(sensor_name, _t) * self)\
{\
    return CONNECT3(g_, sensor_name, _port)->read_pin(self->gpio, self->pin);\
}\

#endif   // !LIBCA_EM_DRIVER_SIMPLE_DIGITAL_SENSOR_TEMPLATE_H
