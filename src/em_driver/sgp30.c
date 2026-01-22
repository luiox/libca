#include "sgp30.h"
#include "../em_base/debug.h"

static const sgp30_port_t* g_sgp30_port = NULL;

void sgp30_bind_port(const sgp30_port_t* port)
{
    g_sgp30_port = port;
}

bool sgp30_port_is_registered(void)
{
    return g_sgp30_port != NULL;
}

// 内部常量（实现细节，仅在 .c 中定义）
#define SGP30_ADDR 0x58
#define SGP30_ADDR_WRITE (SGP30_ADDR << 1)
#define SGP30_ADDR_READ ((SGP30_ADDR << 1) | 1)

#define SGP30_CMD_INIT_AIR_QUALITY 0x2003
#define SGP30_CMD_MEASURE_AIR_QUALITY 0x2008

// 简化宏
#define SGP30_I2C_WRITE(self, buf, len) \
    g_sgp30_port->i2c_write((self)->hi2c, SGP30_ADDR_WRITE, 0, 0, (u8*)(buf), (u16)(len), 0xFFFF)
#define SGP30_I2C_READ(self, buf, len) \
    g_sgp30_port->i2c_read((self)->hi2c, SGP30_ADDR_READ, 0, 0, (u8*)(buf), (u16)(len), 0xFFFF)
#define SGP30_DELAY_MS(ms)                          \
    do {                                            \
        if (g_sgp30_port && g_sgp30_port->delay_ms) \
            g_sgp30_port->delay_ms(ms);             \
    } while (0)

/**
 * @brief	向SGP30发送一条指令(16bit)
 * @param	cmd SGP30指令
 * @retval	成功返回HAL_OK
 */
static i32 sgp30_send_cmd(sgp30_t* self, u16 cmd)
{
    u8 cmd_buffer[2];
    cmd_buffer[0] = (u8)(cmd >> 8);
    cmd_buffer[1] = (u8)(cmd & 0xFF);

    if (!g_sgp30_port) {
        debug_print("[sgp30] port not registered\n");
        return SGP30_ERR_PORT_NOT_REGISTERED;
    }

    if (SGP30_I2C_WRITE(self, cmd_buffer, 2) != 0) {
        debug_print("[sgp30] send_cmd: i2c write fail\n");
        return SGP30_ERR_I2C_FAIL;
    }

    return SGP30_OK;
}

/**
 * @brief	软复位SGP30
 * @param	none
 * @retval	成功返回HAL_OK
 */
static i32 sgp30_soft_reset(sgp30_t* self)
{
    // Soft reset CMD = 0x0006
    return sgp30_send_cmd(self, 0x0006);
}

/**
 * @brief	初始化SGP30空气质量测量模式
 * @param	none
 * @retval	成功返回0，失败返回-1
 */
void sgp30_init(sgp30_t* self, void* hi2c)
{
    self->hi2c = hi2c;

    if (!g_sgp30_port) {
        debug_print("[sgp30] init: port not registered\n");
        return;
    }

    i32 ret = sgp30_soft_reset(self);
    if (ret != SGP30_OK) {
        debug_print("[sgp30] init: soft reset fail\n");
        return;
    }

    SGP30_DELAY_MS(100);

    ret = sgp30_send_cmd(self, SGP30_CMD_INIT_AIR_QUALITY);
    if (ret != SGP30_OK) {
        debug_print("[sgp30] init: init air quality fail\n");
        return;
    }

    SGP30_DELAY_MS(100);
}

/**
 * @brief	初始化SGP30空气质量测量模式
 * @param	none
 * @retval	成功返回HAL_OK
 */
static i32 sgp30_start(sgp30_t* self)
{
    return sgp30_send_cmd(self, SGP30_CMD_MEASURE_AIR_QUALITY);
}

#define CRC8_POLYNOMIAL 0x31

static uint8_t CheckCrc8(uint8_t* const message, uint8_t initial_value)
{
    uint8_t remainder;      // 余数
    uint8_t i = 0, j = 0;   // 循环变量

    /* 初始化 */
    remainder = initial_value;

    for (j = 0; j < 2; j++) {
        remainder ^= message[j];

        /* 从最高位开始依次计算  */
        for (i = 0; i < 8; i++) {
            if (remainder & 0x80) {
                remainder = (remainder << 1) ^ CRC8_POLYNOMIAL;
            }
            else {
                remainder = (remainder << 1);
            }
        }
    }

    /* 返回计算的CRC码 */
    return remainder;
}

/**
 * @brief	读取一次空气质量数据
 * @param	none
 * @retval	成功返回0，失败返回-1
 */
i32 sgp30_read(sgp30_t* self, sgp30_data_t* out)
{
    if (!g_sgp30_port)
        return SGP30_ERR_PORT_NOT_REGISTERED;
    if (!self || !out)
        return SGP30_ERR_I2C_FAIL;

    i32 ret = sgp30_start(self);
    if (ret != SGP30_OK) {
        debug_print("[sgp30] start fail\n");
        return ret;
    }

    SGP30_DELAY_MS(100);

    u8 recv_buffer[6] = {0};
    if (SGP30_I2C_READ(self, recv_buffer, 6) != 0) {
        debug_print("[sgp30] i2c read fail\n");
        return SGP30_ERR_I2C_FAIL;
    }

    if (CheckCrc8(&recv_buffer[0], 0xFF) != recv_buffer[2]) {
        debug_print("[sgp30] co2 crc fail\n");
        return SGP30_ERR_CRC_FAIL;
    }
    if (CheckCrc8(&recv_buffer[3], 0xFF) != recv_buffer[5]) {
        debug_print("[sgp30] tvoc crc fail\n");
        return SGP30_ERR_CRC_FAIL;
    }

    out->co2  = ((u16)recv_buffer[0] << 8) | recv_buffer[1];
    out->tvoc = ((u16)recv_buffer[3] << 8) | recv_buffer[4];

    return SGP30_OK;
}
