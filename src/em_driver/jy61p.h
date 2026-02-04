/**
 * @file jy61p.h
 * @brief JY61P 传感器 (Wit 协议) 驱动头文件
 *
 * 本驱动基于串口通信，解析来自 JY61P 设备的 Wit 协议数据帧（帧头 0x55，类型 0x51~0x54，帧长 11 字节）。
 * 使用步骤：
 *   1. 使用 CubeMX 或其他方式初始化平台串口
 *   2. 使用 `jy61p_bind_port` 注册平台相关的回调（串口发送/延时）
 *   3. 调用 `jy61p_init` 初始化设备实例
 *   4. 读取串口数据并使用 `jy61p_parse_frame` 解析帧，随后使用 `jy61p_get_*` 获取数据
 *
 * @note 请保证串口波特率为 9600、8N1（8 位数据、无校验、1 位停止位）以获得兼容性。
 */
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

/**
 * @brief 绑定平台相关接口
 *
 * 将平台相关的串口发送函数与延时函数绑定到驱动，驱动调用这些函数与平台无关。
 *
 * @param port [in] 平台回调集合，不能为空
 */
void jy61p_bind_port(const jy61p_port_t* port);

/**
 * @brief 检查平台接口是否已注册
 *
 * @return true 已注册
 * @return false 未注册
 */
bool jy61p_port_is_registered(void);

typedef struct jy61p_frame{
    const uint8_t *ptr;  // 帧的起始地址 (指向 user buffer)
    uint8_t       type;  // 0x51 ~ 0x54
} jy61p_frame_t;

typedef struct jy61p
{
	void* huart;
} jy61p_t;

/**
 * @brief 初始化 JY61P 设备实例
 *
 * 该函数仅保存用户传入的串口句柄并准备设备实例，驱动的硬件相关操作通过 `jy61p_bind_port` 提供的回调完成。
 *
 * @param jy61p [in] 设备实例指针，不能为空
 * @param huart [in] 平台串口句柄，不能为空（由应用负责生命周期）
 */
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

/**
 * @brief 从数据帧中解析出加速度
 *
 * @param self  [in] 设备实例指针，不能为空
 * @param frame [in] 已解析的帧指针，不能为空
 * @param acc_x [out] X 轴加速度，单位 m/s^2，不能为空
 * @param acc_y [out] Y 轴加速度，单位 m/s^2，不能为空
 * @param acc_z [out] Z 轴加速度，单位 m/s^2，不能为空
 */
void jy61p_get_acc(jy61p_t* self, jy61p_frame_t *frame,float* acc_x, float* acc_y, float* acc_z);

/**
 * @brief 从数据帧中解析出角速度
 *
 * @param self   [in] 设备实例指针，不能为空
 * @param frame  [in] 已解析的帧指针，不能为空
 * @param gyro_x [out] X 轴角速度，单位 deg/s，不能为空
 * @param gyro_y [out] Y 轴角速度，单位 deg/s，不能为空
 * @param gyro_z [out] Z 轴角速度，单位 deg/s，不能为空
 */
void jy61p_get_gyro(jy61p_t* self, jy61p_frame_t *frame,float* gyro_x, float* gyro_y, float* gyro_z);

/**
 * @brief 从数据帧中解析出角度
 *
 * @param self  [in] 设备实例指针，不能为空
 * @param frame [in] 已解析的帧指针，不能为空
 * @param roll  [out] 翻滚角，单位 deg，不能为空
 * @param pitch [out] 俯仰角，单位 deg，不能为空
 * @param yaw   [out] 偏航角，单位 deg，不能为空
 */
void jy61p_get_angle(jy61p_t* self, jy61p_frame_t *frame,float* roll, float* pitch, float* yaw);

/**
 * @brief 对加速度计进行校准（发送解锁->校准->保存命令序列）
 *
 * @param self [in] 设备实例指针，不能为空
 */
void jy61p_acc_calibration(jy61p_t* self);

/**
 * @brief 将设备 XY 轴置零（发送解锁->XY 置零->保存命令序列）
 *
 * @param self [in] 设备实例指针，不能为空
 */
void jy61p_zero_xy(jy61p_t* self);

/**
 * @brief 将设备 Z 轴（偏航）置零（发送解锁->Z 置零->保存命令序列）
 *
 * @param self [in] 设备实例指针，不能为空
 */
void jy61p_zero_yaw(jy61p_t* self);

#endif   // !MYLIB_DEVICE_JY61P_H
