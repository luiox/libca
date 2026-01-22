#include "dht11.h"
#include "../em_base/debug.h"

static const dht11_port_t* g_dht11_port = NULL;

void dht11_bind_port(const dht11_port_t* port)
{
    g_dht11_port = port;
}

bool dht11_port_is_registered(void)
{
    return g_dht11_port != NULL;
}

// 初始化驱动对象（绑定 gpio/pin）
void dht11_init(dht11_t* self, void* gpio, u16 pin)
{
    if (!self) return;
    self->gpio = gpio;
    self->pin = pin;
}

// 内部时序常量（DHT11，单位us/ms）
#define DHT11_START_LOW_MS         20  // host pull low >=18 ms
#define DHT11_PULLUP_WAIT_US       100
#define DHT11_ACK_WAIT_US_MAX      100
#define DHT11_BIT_WAIT_LOW_US_MAX  100

// 访问宏
#define DHT11_WRITE(self, v)    g_dht11_port->write_pin((self)->gpio, (self)->pin, (v))
#define DHT11_READ(self)        g_dht11_port->read_pin((self)->gpio, (self)->pin)
#define DHT11_OUTPUT_MODE(self) g_dht11_port->set_output_mode((self)->gpio, (self)->pin)
#define DHT11_INPUT_MODE(self)  g_dht11_port->set_input_mode((self)->gpio, (self)->pin)
#define DHT11_DELAY_US(us)      g_dht11_port->delay_us(us)
#define DHT11_DELAY_MS(ms)      g_dht11_port->delay_ms(ms)

// 复位并发起一次测量
static i32 dht11_reset_and_start(dht11_t* self)
{
    if (!g_dht11_port) return DHT11_ERR_PORT_NOT_REGISTERED;

    DHT11_OUTPUT_MODE(self);
    DHT11_WRITE(self, 0);
    DHT11_DELAY_MS(DHT11_START_LOW_MS);
    DHT11_WRITE(self, 1);
    DHT11_DELAY_US(30);

    DHT11_INPUT_MODE(self);
    return DHT11_OK;
}

static i32 dht11_check_response(dht11_t* self)
{
    u32 retry = 0;
    // wait for response low
    while (DHT11_READ(self)) {
        DHT11_DELAY_US(1);
        retry++;
        if (retry >= DHT11_ACK_WAIT_US_MAX) break;
    }
    if (retry >= DHT11_ACK_WAIT_US_MAX) return DHT11_ERR_NO_RESPONSE;

    retry = 0;
    while (!DHT11_READ(self)) {
        DHT11_DELAY_US(1);
        retry++;
        if (retry >= DHT11_ACK_WAIT_US_MAX) break;
    }
    if (retry >= DHT11_ACK_WAIT_US_MAX) return DHT11_ERR_BAD_ACK1;

    retry = 0;
    while (DHT11_READ(self)) {
        DHT11_DELAY_US(1);
        retry++;
        if (retry >= DHT11_ACK_WAIT_US_MAX) break;
    }
    if (retry >= DHT11_ACK_WAIT_US_MAX) return DHT11_ERR_BAD_ACK2;

    return DHT11_OK;
}

static u8 dht11_read_bit(dht11_t* self)
{
    u32 retry = 0;
    while (!DHT11_READ(self)) {
        DHT11_DELAY_US(1);
        retry++;
        if (retry >= DHT11_BIT_WAIT_LOW_US_MAX) break;
    }
    retry = 0;
    while (DHT11_READ(self)) {
        DHT11_DELAY_US(1);
        retry++;
        if (retry >= DHT11_BIT_WAIT_LOW_US_MAX) break;
    }
    DHT11_DELAY_US(40);
    return (DHT11_READ(self) ? 1 : 0);
}

static u8 dht11_read_byte(dht11_t* self)
{
    u8 i;
    u8 dat = 0;
    for (i = 0; i < 8; i++) {
        dat <<= 1;
        dat |= dht11_read_bit(self);
    }
    return dat;
}

// 读取并返回 humidity(0.1%RH) / temperature(0.1C)
i32 dht11_read(dht11_t* self, u16* humidity, i16* temperature)
{
    if (!g_dht11_port) return DHT11_ERR_PORT_NOT_REGISTERED;
    if (!self) return DHT11_ERR_INVALID_PARAM;

    u8 buf[5];
    i32 ret = dht11_reset_and_start(self);
    if (ret != DHT11_OK) return ret;

    ret = dht11_check_response(self);
    if (ret != DHT11_OK) return ret;

    for (int i = 0; i < 5; i++) {
        buf[i] = dht11_read_byte(self);
    }

    if ((u8)(buf[0] + buf[1] + buf[2] + buf[3]) != buf[4]) {
        debug_print("[dht11] checksum fail\n");
        return DHT11_ERR_CHECKSUM_FAIL;
    }

    if (humidity) *humidity = (u16)(buf[0]);
    if (temperature) *temperature = (i16)(buf[2]);

    return DHT11_OK;
}
