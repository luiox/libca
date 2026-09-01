#include "jy61p.h"
#include <em_base/debug.h>

////////////////////////////////////////////////////////////////////////////////

#if (LIBCA_JY61P_PORT_MODE == LIBCA_JY61P_PORT_MODE_EXTERN)
#    define JY61P_UART_SEND(huart, buf, len) port_jy61p_uart_send((huart), (buf), (len))
#    define JY61P_DELAY_MS(ms) port_jy61p_delay_ms(ms)

#elif (LIBCA_JY61P_PORT_MODE == LIBCA_JY61P_PORT_MODE_DYNAMIC)
static const jy61p_port_t* g_jy61p_port = NULL;
#    define JY61P_UART_SEND(huart, buf, len) g_jy61p_port->uart_send((huart), (buf), (len))
#    define JY61P_DELAY_MS(ms) g_jy61p_port->delay_ms(ms)

#else
#    error "Invalid JY61P port mode"
#endif

#if (LIBCA_JY61P_PORT_MODE == LIBCA_JY61P_PORT_MODE_DYNAMIC)
void jy61p_bind_port(const jy61p_port_t* port)
{
    g_jy61p_port = port;
}
bool jy61p_port_is_registered(void)
{
    return g_jy61p_port != NULL;
}
#endif

////////////////////////////////////////////////////////////////////////////////

void jy61p_init(jy61p_t* jy61p, void* huart)
{
    param_check(jy61p != NULL);
    param_check(huart != NULL);

    jy61p->huart = huart;
}

// Wit协议：https://wit-motion.yuque.com/wumwnr/ltst03/wegquy
// 数据帧头0x55 类型0x51~0x54 数据低8位 数据高8位 数据低8位 数据高8位 数据低8位 数据高8位 数据低8位
// 数据高8位 SUMCRC（16位）
// 55 51 9F FD E8 FF A5 07 35 0B 15 加速度
// 55 52 00 00 00 00 00 00 35 0B E7 角速度
// 55 53 69 21 7D F8 E8 C4 82 46 1B 角度
// 55 54 00 00 00 00 00 00 00 00 A9 磁场

i32 jy61p_parse_frame(jy61p_t* self, const u8* ptr, usize total_len, jy61p_frame_t* out_frame)
{
    /* 参数校验（契约式） */
    param_check(self != NULL);
    param_check(ptr != NULL);
    param_check(out_frame != NULL);

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

    // 4. 校验和
    u8 calc_sum = 0;
    for (int i = 0; i < 10; i++)
        calc_sum += ptr[i];
    u8 recv_sum = ptr[10];

    if (calc_sum != recv_sum)
        return JY61P_ERR_CHECKSUM;

    // 5. 成功！
    out_frame->ptr  = ptr;
    out_frame->type = type;

    // 6. 返回消费长度
    return 11;
}

/*
 * @brief 私有：将高低字节合成为 i16
 *
 * 注意先将高字节转换为 i16 再左移，避免溢出，符合 Wit 数据格式。
 */
static inline i16 combine_byte_to_short(uint8_t high, uint8_t low)
{
    return ((i16)high << 8) | low;
}

// 获取加速度（单位：m/s^2）
void jy61p_get_acc(jy61p_t* jy61p, jy61p_frame_t* frame, f32* acc_x, f32* acc_y, f32* acc_z)
{
    /* 参数校验（契约式） */
    param_check(jy61p != NULL);
    param_check(frame != NULL);
    param_check(acc_x != NULL);
    param_check(acc_y != NULL);
    param_check(acc_z != NULL);

    // 解析数据
    // fAcc[i] = sReg[AX+i] / 32768.0f * 16.0f * g; // g为重力加速度，这里假设为9.8m/s^2
    // 重力加速度G
    static const f32 gravitational_acceleration = 9.8f;

    uint8_t low, high;
    low     = frame->ptr[2];
    high    = frame->ptr[3];
    i16 tmp = combine_byte_to_short(high, low);
    *acc_x  = (f32)tmp / 32768.0f * 16.0f * gravitational_acceleration;

    low    = frame->ptr[4];
    high   = frame->ptr[5];
    tmp    = combine_byte_to_short(high, low);
    *acc_y = (f32)tmp / 32768.0f * 16.0f * gravitational_acceleration;

    low    = frame->ptr[6];
    high   = frame->ptr[7];
    tmp    = combine_byte_to_short(high, low);
    *acc_z = (f32)tmp / 32768.0f * 16.0f * gravitational_acceleration;

    // low = jy61p->acc_frame[8];
    // high = jy61p->acc_frame[9];
    // tmp = (i16)(high << 8) | (i16)low;
    // *acc_temperature = (f32)tmp / 100.0f;
}

// 获取角速度（单位：deg/s）
void jy61p_get_gyro(jy61p_t* jy61p, jy61p_frame_t* frame, f32* gyro_x, f32* gyro_y, f32* gyro_z)
{
    /* 参数校验（契约式） */
    param_check(jy61p != NULL);
    param_check(frame != NULL);
    param_check(gyro_x != NULL);
    param_check(gyro_y != NULL);
    param_check(gyro_z != NULL);

    // 解析数据
    // fGyro[i] = sReg[GX+i] / 32768.0f * 2000.0f;

    uint8_t low, high;
    low     = frame->ptr[2];
    high    = frame->ptr[3];
    i16 tmp = combine_byte_to_short(high, low);
    *gyro_x = (f32)tmp / 32768.0f * 2000.0f;

    low     = frame->ptr[4];
    high    = frame->ptr[5];
    tmp     = combine_byte_to_short(high, low);
    *gyro_y = (f32)tmp / 32768.0f * 2000.0f;

    low     = frame->ptr[6];
    high    = frame->ptr[7];
    tmp     = combine_byte_to_short(high, low);
    *gyro_z = (f32)tmp / 32768.0f * 2000.0f;
}

// 获取角度（单位：deg）
void jy61p_get_angle(jy61p_t* jy61p, jy61p_frame_t* frame, f32* roll, f32* pitch, f32* yaw)
{
    /* 参数校验（契约式） */
    param_check(jy61p != NULL);
    param_check(frame != NULL);
    param_check(roll != NULL);
    param_check(pitch != NULL);
    param_check(yaw != NULL);

    // 解析数据
    // fAngle[i] = sReg[Roll+i] / 32768.0f * 180.0f;

    // jy61p_print_data_frame(jy61p->angle_frame, 11);

    uint8_t low, high;
    low     = frame->ptr[2];
    high    = frame->ptr[3];
    i16 tmp = combine_byte_to_short(high, low);
    *roll   = (f32)tmp / 32768.0f * 180.0f;

    low    = frame->ptr[4];
    high   = frame->ptr[5];
    tmp    = combine_byte_to_short(high, low);
    *pitch = (f32)tmp / 32768.0f * 180.0f;

    low  = frame->ptr[6];
    high = frame->ptr[7];
    tmp  = combine_byte_to_short(high, low);
    *yaw = (f32)tmp / 32768.0f * 180.0f;
}

static const u8 JY61P_ULOCK_CMD[5] = {0xFF, 0xAA, 0x69, 0x88, 0xB5};   // 解锁
static const u8 JY61P_SAVE_CMD[5]  = {0xFF, 0xAA, 0x00, 0x00, 0x00};   // 保存
static const u8 JY61P_ACC_CMD[5]   = {0xFF, 0xAA, 0x01, 0x01, 0x00};   // 加速度校准
static const u8 JY61P_XY0_CMD[5]   = {0xFF, 0xAA, 0x01, 0x08, 0x00};   // XY轴归零
static const u8 JY61P_Z0_CMD[5]    = {0xFF, 0xAA, 0x01, 0x04, 0x00};   // Z轴归零
#define jy61p_send_cmd(cmd, len) JY61P_UART_SEND(self->huart, (const u8*)(cmd), (len))
#define jy61p_delay_ms(ms) JY61P_DELAY_MS(ms)

// 校准加速度器
void jy61p_acc_calibration(jy61p_t* self)
{
    param_check(self != NULL);

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
    param_check(self != NULL);

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
    param_check(self != NULL);

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
