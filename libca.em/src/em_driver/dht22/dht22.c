#include "dht22.h"

////////////////////////////////////////////////////////////////////////////////

#if (LIBCA_DHT22_PORT_MODE == LIBCA_DHT22_PORT_MODE_EXTERN)
#define DHT_WRITE(self, v)      port_dht22_write_pin((self)->gpio, (self)->pin, (v))
#define DHT_READ(self)          port_dht22_read_pin((self)->gpio, (self)->pin)
#define DHT_OUTPUT_MODE(self)   port_dht22_set_output_mode((self)->gpio, (self)->pin)
#define DHT_INPUT_MODE(self)    port_dht22_set_input_mode((self)->gpio, (self)->pin)
#define DHT_DELAY_US(us)        port_dht22_delay_us(us)
#define DHT_DELAY_MS(ms)        port_dht22_delay_ms(ms)

#elif (LIBCA_DHT22_PORT_MODE == LIBCA_DHT22_PORT_MODE_DYNAMIC)
static const dht22_port_t* g_dht22_port = NULL;
#define DHT_WRITE(self, v)      g_dht22_port->write_pin((self)->gpio, (self)->pin, (v))
#define DHT_READ(self)          g_dht22_port->read_pin((self)->gpio, (self)->pin)
#define DHT_OUTPUT_MODE(self)   g_dht22_port->set_output_mode((self)->gpio, (self)->pin)
#define DHT_INPUT_MODE(self)    g_dht22_port->set_input_mode((self)->gpio, (self)->pin)
#define DHT_DELAY_US(us)        g_dht22_port->delay_us(us)
#define DHT_DELAY_MS(ms)        g_dht22_port->delay_ms(ms)

#else
#error "Invalid DHT22 port mode"
#endif

#if (LIBCA_DHT22_PORT_MODE == LIBCA_DHT22_PORT_MODE_DYNAMIC)
void dht22_bind_port(const dht22_port_t* port) { g_dht22_port = port; }
bool dht22_port_is_registered(void) { return g_dht22_port != NULL; }
#endif

////////////////////////////////////////////////////////////////////////////////

// 内部缓冲和变量
static uint16_t bits[40];
static u8 hMSB, hLSB, tMSB, tLSB, parity_rcv;

// 实现细节常量（时序，单位 us）
#define DHT22_START_LOW_MS      2   // host start low pulse >= 800us (use 2ms)
#define DHT22_WAIT_PRESENCE_MAX_US  400
#define DHT22_ACK_MIN_US        8
#define DHT22_ACK_MAX_US        15
#define DHT22_BIT_WAIT_LOW_MAX_US 20
#define DHT22_BIT_HIGH_ONE_THRESHOLD_US 16

// 访问宏
// 读取原始 40 bit（内部函数）
static i32 dht22_read_bits(dht22_t* self)
{
    u32 wait;
    u8 i;

    // 1. 发起主机拉低脉冲
    DHT_OUTPUT_MODE(self);
    DHT_WRITE(self, 0);
    DHT_DELAY_MS(DHT22_START_LOW_MS);
    DHT_WRITE(self, 1);

    // 切换为输入，等待器件响应
    DHT_INPUT_MODE(self);

    // 等待 presence（高电平结束）
    wait = 0;
    while (DHT_READ(self) && (wait++ < (DHT22_WAIT_PRESENCE_MAX_US / 2))) {
        DHT_DELAY_US(2);
    }
    if (wait > (DHT22_WAIT_PRESENCE_MAX_US / 2)) return DHT22_ERR_NO_RESPONSE;

    // 检查 ACK 低脉冲
    wait = 0;
    while (!DHT_READ(self) && (wait++ < DHT22_ACK_MAX_US)) DHT_DELAY_US(1);
    if ((wait < DHT22_ACK_MIN_US) || (wait > DHT22_ACK_MAX_US)) return DHT22_ERR_BAD_ACK1;

    // ACK 高脉冲
    wait = 0;
    while (DHT_READ(self) && (wait++ < DHT22_ACK_MAX_US)) DHT_DELAY_US(1);
    if ((wait < DHT22_ACK_MIN_US) || (wait > DHT22_ACK_MAX_US)) return DHT22_ERR_BAD_ACK2;

    // 开始读取 40 位
    i = 0;
    while (i < 40) {
        // 等待低电平开始
        wait = 0;
        while (!DHT_READ(self) && (wait++ < DHT22_BIT_WAIT_LOW_MAX_US)) DHT_DELAY_US(1);
        if (wait > DHT22_BIT_WAIT_LOW_MAX_US) {
            bits[i] = 0xFFFF;
            // 尝试跳过当前高电平
            wait = 0;
            while (DHT_READ(self) && (wait++ < DHT22_BIT_WAIT_LOW_MAX_US)) DHT_DELAY_US(1);
        } else {
            // 测量高电平长度
            wait = 0;
            while (DHT_READ(self) && (wait++ < DHT22_BIT_WAIT_LOW_MAX_US)) DHT_DELAY_US(1);
            bits[i] = (wait < DHT22_BIT_HIGH_ONE_THRESHOLD_US) ? wait : 0xFFFF;
        }
        i++;
    }

    for (i = 0; i < 40; i++) if (bits[i] == 0xFFFF) return DHT22_ERR_TIMEOUT;

    return DHT22_OK;
}

// 解码并校验，输出 humidity (0.1%RH) 与 temperature (0.1°C, 带符号)
static i32 dht22_decode(u16* humidity10, i16* temperature10)
{
    u8 parity;
    u8 i = 0;

    hMSB = 0;
    for (; i < 8; i++) { hMSB <<= 1; if (bits[i] > 7) hMSB |= 1; }
    hLSB = 0;
    for (; i < 16; i++) { hLSB <<= 1; if (bits[i] > 7) hLSB |= 1; }
    tMSB = 0;
    for (; i < 24; i++) { tMSB <<= 1; if (bits[i] > 7) tMSB |= 1; }
    tLSB = 0;
    for (; i < 32; i++) { tLSB <<= 1; if (bits[i] > 7) tLSB |= 1; }
    parity_rcv = 0;
    for (; i < 40; i++) { parity_rcv <<= 1; if (bits[i] > 7) parity_rcv |= 1; }

    parity = hMSB + hLSB + tMSB + tLSB;
    if ((u8)(parity & 0xFF) != parity_rcv) return DHT22_ERR_CRC_FAIL;

    if (humidity10) *humidity10 = (u16)((hMSB << 8) | hLSB);

    if (temperature10) {
        // DHT22 温度高位为符号位
        i16 tmp = (i16)(((u16)tMSB << 8) | tLSB);
        if (tmp & 0x8000) {
            // 负数处理
            tmp = -(tmp & 0x7FFF);
        }
        *temperature10 = tmp;
    }

    return DHT22_OK;
}

// 公共接口：读取温湿度
i32 dht22_read(dht22_t* self, u16* humidity10, i16* temperature10)
{
    if (!self) return DHT22_ERR_INVALID_PARAM;

    i32 ret = dht22_read_bits(self);
    if (ret != DHT22_OK) return ret;

    return dht22_decode(humidity10, temperature10);
}