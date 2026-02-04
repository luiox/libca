// 本驱动目前只支持iic驱动jy61p
//
// 使用方法：
//     1. 使用CubeMX初始化一个串口出来
//     2. 调用jy61p_init进行初始化设备
//     3. 调用jy61p_xxx读取数据

// 配置串口3
// 填写9600波特率、8位帧长、无校验位、1位停止位
#ifndef MYLIB_DEVICE_JY61P_H
#define MYLIB_DEVICE_JY61P_H

#include "../em_base/datatype.h"
#include <stdbool.h>

// 如果没有修改，那么默认就是0x50
#define JY61P_DEFAULT_ADDRESS 0x50

// 定义一些清晰的错误码 (可选，直接返回负数也行)
#define JY61P_ERR_SHORT   -1  // 数据包长度不足
#define JY61P_ERR_HEADER  -2  // 帧头错误
#define JY61P_ERR_TYPE    -3  // 类型不支持
#define JY61P_ERR_CRC     -4  // 校验和错误

typedef struct jy61p_port{
	i32 (*uart_send)(void* huart, u8* buf, usize len);
	void (*delay_ms)(u32 ms);
}jy61p_port_t;

void jy61p_bind_port(const jy61p_port_t* port);
bool jy61p_port_is_registered(void);

typedef struct jy61p_frame{
    const uint8_t *ptr;  // 帧的起始地址 (指向 user buffer)
    uint8_t       type;  // 0x51 ~ 0x54
} jy61p_frame_t;

typedef struct jy61p
{
	void* huart;
} jy61p_t;

// 初始化
void jy61p_init(jy61p_t* jy61p, void* huart);

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
i32 jy61p_parse_frame(jy61p_t* self, const u8 *ptr, usize total_len, jy61p_frame_t *out_frame);

// 获取加速度
void jy61p_get_acc(jy61p_t* self, jy61p_frame_t *frame,float* acc_x, float* acc_y, float* acc_z);

// 获取角速度
void jy61p_get_gyro(jy61p_t* self, jy61p_frame_t *frame,float* gyro_x, float* gyro_y, float* gyro_z);

// 获取角度
void jy61p_get_angle(jy61p_t* self, jy61p_frame_t *frame,float* roll, float* pitch, float* yaw);

// 校准加速度器
void jy61p_acc_calibration(jy61p_t* self);

// xy轴置零
void jy61p_zero_xy(jy61p_t* self);

// z轴置零
void jy61p_zero_yaw(jy61p_t* self);

#endif   // !MYLIB_DEVICE_JY61P_H
