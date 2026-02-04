#include "jy61p.h"
#include "../em_base/debug.h"

static const jy61p_port_t* g_jy61p_port = NULL;

void jy61p_bind_port(const jy61p_port_t* port)
{
    param_check(port != NULL);
    g_jy61p_port = port;
}

bool jy61p_port_is_registered(void)
{
    return g_jy61p_port != NULL;
}

// Wit协议：https://wit-motion.yuque.com/wumwnr/ltst03/wegquy
// 数据帧头0x55 类型0x51~0x54 数据低8位 数据高8位 数据低8位 数据高8位 数据低8位 数据高8位 数据低8位
// 数据高8位 SUMCRC（16位） 55 51 9F FD E8 FF A5 07 35 0B 15 加速度 55 52 00 00 00 00 00 00 35 0B E7
// 角速度 55 53 69 21 7D F8 E8 C4 82 46 1B 角度 55 54 00 00 00 00 00 00 00 00 A9 磁场

/**
 * @brief 尝试解析当前位置的一帧数据
 *
 * @param ptr       [In]  当前 buffer 的起始指针
 * @param total_len [In]  buffer 的总长度
 * @param out_frame [Out] 解析成功后的帧信息
 * @return i32      状态码
 *                  > 0 : 成功解析，返回值为消费的长度 (通常是 11)
 *                  -1  : 数据不足 (建议缓存起来，等下一次数据)
 *                  -2  : 帧头/数据错误 (建议跳过当前字节 ptr++)
 */
i32 jy61p_parse_frame(jy61p_t* self, const u8* ptr, usize total_len, jy61p_frame_t* out_frame)
{
    // 1. 长度检查
    if (total_len < 11)
        return JY61P_ERR_SHORT;

    // 2. 帧头检查
    if (ptr[0] != 0x55)
        return JY61P_ERR_HEADER;

    // 3. 类型检查
    u8 type = ptr[1];
    if (type < 0x51 || type > 0x54)
        return JY61P_ERR_TYPE;

    // 4. 校验和 (优化：这里可以用查表法或者 unroll 优化，但目前这样最清晰)
    u16 calc_sum = 0;
    for (int i = 0; i < 10; i++)
        calc_sum += ptr[i];
    u16 recv_sum = (u16)ptr[10] << 8 | ptr[11];

    if (calc_sum != recv_sum)
        return JY61P_ERR_CRC;

    // 5. 成功！
    out_frame->ptr  = ptr;
    out_frame->type = type;

    // 6. 返回消费长度
    return 11;
}

// 合成字节到短整型
// 这里必须是先转为int16_t，再合成，合成完了重新转int16_t
// 参考https://wit-motion.yuque.com/wumwnr/ltst03/vl3tpy?#tBLhV
static inline int16_t combine_byte_to_short(uint8_t high, uint8_t low)
{
    return ((int16_t)high << 8) | low;
}

// 获取加速度
void jy61p_get_acc(jy61p_t* jy61p, jy61p_frame_t* frame, float* acc_x, float* acc_y, float* acc_z)
{
    // 解析数据
    // fAcc[i] = sReg[AX+i] / 32768.0f * 16.0f * g; // g为重力加速度，这里假设为9.8m/s^2
    // 重力加速度G
    static const float gravitational_acceleration = 9.8f;

    uint8_t low, high;
    low         = frame->ptr[2];
    high        = frame->ptr[3];
    int16_t tmp = combine_byte_to_short(high, low);
    *acc_x      = (float)tmp / 32768.0f * 16.0f * gravitational_acceleration;

    low    = frame->ptr[4];
    high   = frame->ptr[5];
    tmp    = combine_byte_to_short(high, low);
    *acc_y = (float)tmp / 32768.0f * 16.0f * gravitational_acceleration;

    low    = frame->ptr[6];
    high   = frame->ptr[7];
    tmp    = combine_byte_to_short(high, low);
    *acc_z = (float)tmp / 32768.0f * 16.0f * gravitational_acceleration;

    // low = jy61p->acc_frame[8];
    // high = jy61p->acc_frame[9];
    // tmp = (int16_t)(high << 8) | (int16_t)low;
    // *acc_temperature = (float)tmp / 100.0f;
}

// 获取角速度
void jy61p_get_gyro(jy61p_t* jy61p, jy61p_frame_t* frame, float* gyro_x, float* gyro_y,
                    float* gyro_z)
{
    // 解析数据
    // fGyro[i] = sReg[GX+i] / 32768.0f * 2000.0f;

    uint8_t low, high;
    low         = frame->ptr[2];
    high        = frame->ptr[3];
    int16_t tmp = combine_byte_to_short(high, low);
    *gyro_x     = (float)tmp / 32768.0f * 2000.0f;

    low     = frame->ptr[4];
    high    = frame->ptr[5];
    tmp     = combine_byte_to_short(high, low);
    *gyro_y = (float)tmp / 32768.0f * 2000.0f;

    low     = frame->ptr[6];
    high    = frame->ptr[7];
    tmp     = combine_byte_to_short(high, low);
    *gyro_z = (float)tmp / 32768.0f * 2000.0f;
}

// 获取角度
void jy61p_get_angle(jy61p_t* jy61p, jy61p_frame_t* frame, float* roll, float* pitch, float* yaw)
{
    // 解析数据
    // fAngle[i] = sReg[Roll+i] / 32768.0f * 180.0f;

    // jy61p_print_data_frame(jy61p->angle_frame, 11);

    uint8_t low, high;
    low         = frame->ptr[2];
    high        = frame->ptr[3];
    int16_t tmp = combine_byte_to_short(high, low);
    *roll       = (float)tmp / 32768.0f * 180.0f;

    low    = frame->ptr[4];
    high   = frame->ptr[5];
    tmp    = combine_byte_to_short(high, low);
    *pitch = (float)tmp / 32768.0f * 180.0f;

    low  = frame->ptr[6];
    high = frame->ptr[7];
    tmp  = combine_byte_to_short(high, low);
    *yaw = (float)tmp / 32768.0f * 180.0f;
}

static uint8_t JY61P_ULOCK_CMD[5] = {0xFF, 0xAA, 0x69, 0x88, 0xB5};   // 解锁
static uint8_t JY61P_SAVE_CMD[5]  = {0xFF, 0xAA, 0x00, 0x00, 0x00};   // 保存
static uint8_t JY61P_ACC_CMD[5]   = {0xFF, 0xAA, 0x01, 0x01, 0x00};   // 加速度校准
static uint8_t JY61P_XY0_CMD[5]   = {0xFF, 0xAA, 0x01, 0x08, 0x00};   // XY轴归零
static uint8_t JY61P_Z0_CMD[5]    = {0xFF, 0xAA, 0x01, 0x04, 0x00};   // Z轴归零

#define jy61p_send_cmd(cmd, len) g_jy61p_port->uart_send(self->huart, cmd, (len))
#define jy61p_delay_ms(ms) g_jy61p_port->delay_ms(ms)

// 校准加速度器
void jy61p_acc_calibration(jy61p_t* self)
{
    // 发送解锁
    jy61p_send_cmd(JY61P_ULOCK_CMD, sizeof(JY61P_ULOCK_CMD));
    // delay 200ms
    jy61p_delay_ms(200);
    // 发送加速度校准
    jy61p_send_cmd(JY61P_ACC_CMD, sizeof(JY61P_ACC_CMD));
    // delay 5000ms
    jy61p_delay_ms(5000);
    // 发送保存
    jy61p_send_cmd(JY61P_SAVE_CMD, sizeof(JY61P_SAVE_CMD));
}

// xy轴置零
void jy61p_zero_xy(jy61p_t* self)
{
    // 发送解锁
    jy61p_send_cmd(JY61P_ULOCK_CMD, sizeof(JY61P_ULOCK_CMD));
    // delay 200ms
    jy61p_delay_ms(200);
    // 发送加速度校准
    jy61p_send_cmd(JY61P_XY0_CMD, sizeof(JY61P_XY0_CMD));
    // delay 3000ms
    jy61p_delay_ms(3000);
    // 发送保存
    jy61p_send_cmd(JY61P_SAVE_CMD, sizeof(JY61P_SAVE_CMD));
}

// z轴置零
void jy61p_zero_yaw(jy61p_t* self)
{
    // 参考：https://blog.csdn.net/m0_52011717/article/details/138538530
    // 发送解锁
    jy61p_send_cmd(JY61P_ULOCK_CMD, sizeof(JY61P_ULOCK_CMD));
    // delay 200ms
    jy61p_delay_ms(200);
    // 发送加速度校准
    jy61p_send_cmd(JY61P_Z0_CMD, sizeof(JY61P_Z0_CMD));
    // delay 3000ms
    jy61p_delay_ms(3000);
    // 发送保存
    jy61p_send_cmd(JY61P_SAVE_CMD, sizeof(JY61P_SAVE_CMD));
}

///////////////////////////////////////////////////////////////////////////////

// 测试代码

#if 0

jy61p_t            jy61p;
extern UART_HandleTypeDef huart1;

void serial_send_byte(uint8_t byte)
{
    HAL_UART_Transmit(&huart1, &byte, 1, 1000);
}

void delay_ms(int32_t ms)
{
    HAL_Delay(ms);
}

void jy61p_test(void)
{
    // 初始化串口，这里假设已经初始化好了huart1
    jy61p_init(&jy61p, &huart1, serial_send_byte, delay_ms);
    // xy轴置零
    jy61p_zero_xy(&jy61p);
    // z轴置零
    jy61p_zero_yaw(&jy61p);

    while (1) {
        // 读取数据
        // 加速度数据
        float ax, ay, az;
        jy61p_get_acc(&jy61p, &ax, &ay, &az);
        DEBUG_INFO("ax:%.2f ay:%.2f az:%.2f", ax, ay, az);
        // 角速度数据
        float gx, gy, gz;
        jy61p_get_gyro(&jy61p, &gx, &gy, &gz);
        DEBUG_INFO("gx:%.2f gy:%.2f gz:%.2f", gx, gy, gz);
        // 角度数据
        float roll, pitch, yaw;
        jy61p_get_angle(&jy61p, &roll, &pitch, &yaw);
        DEBUG_INFO("roll:%.2f pitch:%.2f yaw:%.2f", roll, pitch, yaw);
    }
}

void USART1_IRQHandler(void)
{
    HAL_UART_IRQHandler(&huart1);
    jy61p_irq_handler(&jy61p);
}

#endif

////////////////////////////////////////////////////////////////////////////////
