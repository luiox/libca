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

    // 先确保输出模式并拉高2ms，让DHT11复位到空闲状态
    // 这在中断通信后很重要，可以让DHT11从任何异常状态恢复
    DHT11_OUTPUT_MODE(self);
    DHT11_WRITE(self, 1);
    DHT11_DELAY_MS(2);

    // 主机拉低至少18ms
    DHT11_WRITE(self, 0);
    DHT11_DELAY_MS(18);

    // 总线拉高，主机延时30us
    DHT11_WRITE(self, 1);
    DHT11_DELAY_US(30);

    // 主机设为输入，判断从机响应信号
    DHT11_INPUT_MODE(self);
    return DHT11_OK;
}

static i32 dht11_check_response(dht11_t* self)
{
    volatile u32 timeout;
    
    // 判断从机是否有低电平响应信号，如不响应则跳出
    if (DHT11_READ(self)) {
        return DHT11_ERR_NO_RESPONSE;
    }
    
    // 轮询直到从机发出的80us低电平响应信号结束
    timeout = 0;
    while (!DHT11_READ(self)) {
        if (++timeout > DHT11_MAX_TIMEOUT) {
            return DHT11_ERR_BAD_ACK1;
        }
    }
    
    // 轮询直到从机发出的80us高电平标志信号结束
    timeout = 0;
    while (DHT11_READ(self)) {
        if (++timeout > DHT11_MAX_TIMEOUT) {
            return DHT11_ERR_BAD_ACK2;
        }
    }

    return DHT11_OK;
}

static u8 dht11_read_bit(dht11_t* self)
{
    volatile u32 timeout;
    
    // 每bit以50us低电平开始，轮询直到低电平结束
    // 数据0：50us低 + 26~28us高
    // 数据1：50us低 + 70us高
    timeout = 0;
    while (!DHT11_READ(self)) {
        if (++timeout > DHT11_MAX_TIMEOUT) {
            return 0; // 超时，假设为数据0
        }
    }
    
    // DHT11在26~28us的高电平表示数据0，70us的高电平表示数据1
    // 通过延时x us后的电平状态来判断（x要大于28us且小于70us）
    DHT11_DELAY_US(40); // 延时40us
    
    // x us后仍为高电平表示数据1
    u8 bit = DHT11_READ(self) ? 1 : 0;
    
    // 如果是数据1，等待高电平结束（防止影响下一位读取）
    if (bit) {
        timeout = 0;
        while (DHT11_READ(self)) {
            if (++timeout > DHT11_MAX_TIMEOUT) {
                break; // 超时，直接退出
            }
        }
    }
    
    return bit;
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
    i32 ret;

    // 步骤1: 发送起始信号
    ret = dht11_reset_and_start(self);
    if (ret != DHT11_OK) {
        // 错误时恢复总线状态
        goto cleanup;
    }

    // 步骤2: 检查DHT11响应
    ret = dht11_check_response(self);
    if (ret != DHT11_OK) {
        goto cleanup;
    }

    // 步骤3: 读取40位数据
    for (int i = 0; i < 5; i++) {
        buf[i] = dht11_read_byte(self);
    }

    // 步骤4: 校验数据
    if ((u8)(buf[0] + buf[1] + buf[2] + buf[3]) != buf[4]) {
        debug_print("[dht11] checksum fail\n");
        ret = DHT11_ERR_CHECKSUM_FAIL;
        goto cleanup;
    }

    // 成功，解析数据
    // DHT11数据格式：buf[0]=湿度整数, buf[1]=湿度小数(0), buf[2]=温度整数, buf[3]=温度小数(0)
    if (humidity) *humidity = (u16)(buf[0]) * 10;      // 转换为0.1%单位
    if (temperature) *temperature = (i16)(buf[2]) * 10; // 转换为0.1°C单位
    
    ret = DHT11_OK;

cleanup:
    // 无论成功还是失败，都将总线设为输出并拉高
    // 这确保下次通信从确定状态开始，提高健壮性
    DHT11_OUTPUT_MODE(self);
    DHT11_WRITE(self, 1);

    return ret;
}
