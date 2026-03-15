/**
 * @file jy61p.h
 * @author canrad (1517807724@qq.com)
 * @brief  JY61P 传感器 (Wit 协议) 驱动头文件，已实物验证
 *
 * 本驱动基于串口通信，解析来自 JY61P 设备的 Wit 协议数据帧（帧头 0x55，类型 0x51~0x54，帧长 11
 * 字节）。 使用步骤：
 *   1. 使用 CubeMX 或其他方式初始化平台串口
 *   2. 使用 `jy61p_bind_port` 注册平台相关的回调（串口发送/延时）
 *   3. 调用 `jy61p_init` 初始化设备实例
 *   4. 读取串口数据并使用 `jy61p_parse_frame` 解析帧，随后使用 `jy61p_get_*` 获取数据
 *
 * @note 请保证串口波特率为 9600、8N1（8 位数据、无校验、1 位停止位）以获得兼容性。
 * @version 0.1
 * @date 2026-02-04
 * @update 0.2 添加extern外部依赖注入模式
 *
 * @copyright Copyright (c) 2026
 *
 */
#ifndef LIBCA_EM_DRIVER_JY61P_H
#define LIBCA_EM_DRIVER_JY61P_H

#include <em_base/datatype.h>

// 外部模式
#define LIBCA_JY61P_PORT_MODE_EXTERN 1
// 动态模式
#define LIBCA_JY61P_PORT_MODE_DYNAMIC 2

#ifndef LIBCA_JY61P_PORT_MODE
#define LIBCA_JY61P_PORT_MODE LIBCA_JY61P_PORT_MODE_EXTERN
#endif

#ifdef __cplusplus
extern "C" {
#endif

// 如果没有修改，那么默认就是0x50
#define JY61P_DEFAULT_ADDRESS 0x50

// 类型
#define JY61P_TYPE_ACC 0x51   /* 加速度 */
#define JY61P_TYPE_GYRO 0x52  /* 角速度 */
#define JY61P_TYPE_ANGLE 0x53 /* 角度 */
#define JY61P_TYPE_MAG 0x54   /* 磁场 */

// 错误码
#define JY61P_ERR_SHORT -1    /* 数据包长度不足 */
#define JY61P_ERR_HEADER -2   /* 帧头错误 */
#define JY61P_ERR_TYPE -3     /* 类型不支持 */
#define JY61P_ERR_CHECKSUM -4 /* 校验和错误 */

#if (LIBCA_JY61P_PORT_MODE == LIBCA_JY61P_PORT_MODE_EXTERN)
/**
 * @brief UART发送数据
 * @param huart UART句柄
 * @param buf 发送缓冲区
 * @param len 数据长度
 * @return 0=成功
 */
extern i32 port_jy61p_uart_send(void* huart, const u8* buf, usize len);
/**
 * @brief 毫秒延时
 * @param ms 延时时间（毫秒）
 */
extern void port_jy61p_delay_ms(u32 ms);

#elif (LIBCA_JY61P_PORT_MODE == LIBCA_JY61P_PORT_MODE_DYNAMIC)
typedef struct jy61p_port {
    i32  (*uart_send)(void* huart, const u8* buf, usize len);  // UART发送
    void (*delay_ms)(u32 ms);                                   // 毫秒延时
} jy61p_port_t;
void jy61p_bind_port(const jy61p_port_t* port);
bool jy61p_port_is_registered(void);

#else
#error "Invalid JY61P port mode"
#endif

typedef struct jy61p_frame
{
    const u8* ptr;    // 帧的起始地址 (指向 user buffer)
    u8        type;   // 帧类型 (JY61P_TYPE_*)
} jy61p_frame_t;

typedef struct jy61p
{
    void* huart;
} jy61p_t;

/**
 * @brief 初始化 JY61P 设备实例
 *
 * 该函数仅保存用户传入的串口句柄并准备设备实例，驱动的硬件相关操作通过 `jy61p_bind_port`
 * 提供的回调完成。
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
i32 jy61p_parse_frame(jy61p_t* self, const u8* ptr, usize total_len, jy61p_frame_t* out_frame);

/**
 * @brief 从数据帧中解析出加速度
 *
 * @param self  [in] 设备实例指针，不能为空
 * @param frame [in] 已解析的帧指针，不能为空
 * @param acc_x [out] X 轴加速度，单位 m/s^2，不能为空
 * @param acc_y [out] Y 轴加速度，单位 m/s^2，不能为空
 * @param acc_z [out] Z 轴加速度，单位 m/s^2，不能为空
 */
void jy61p_get_acc(jy61p_t* self, jy61p_frame_t* frame, f32* acc_x, f32* acc_y, f32* acc_z);

/**
 * @brief 从数据帧中解析出角速度
 *
 * @param self   [in] 设备实例指针，不能为空
 * @param frame  [in] 已解析的帧指针，不能为空
 * @param gyro_x [out] X 轴角速度，单位 deg/s，不能为空
 * @param gyro_y [out] Y 轴角速度，单位 deg/s，不能为空
 * @param gyro_z [out] Z 轴角速度，单位 deg/s，不能为空
 */
void jy61p_get_gyro(jy61p_t* self, jy61p_frame_t* frame, f32* gyro_x, f32* gyro_y, f32* gyro_z);

/**
 * @brief 从数据帧中解析出角度
 *
 * @param self  [in] 设备实例指针，不能为空
 * @param frame [in] 已解析的帧指针，不能为空
 * @param roll  [out] 翻滚角，单位 deg，不能为空
 * @param pitch [out] 俯仰角，单位 deg，不能为空
 * @param yaw   [out] 偏航角，单位 deg，不能为空
 */
void jy61p_get_angle(jy61p_t* self, jy61p_frame_t* frame, f32* roll, f32* pitch, f32* yaw);

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

#ifdef __cplusplus
}
#endif

#endif   // !LIBCA_EM_DRIVER_JY61P_H
