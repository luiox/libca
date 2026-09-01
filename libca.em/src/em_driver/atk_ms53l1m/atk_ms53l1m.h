/// @file atkms53l1m.h
/// @author Canrad
/// @brief ATK-MS53L1M模块驱动代码
/// @version 0.2
/// @date 2026-02-03
/// @update 0.2 添加extern外部依赖注入模式
///
/// @copyright Copyright (c) 2026
///
///
#ifndef LIBCA_EM_DRIVER_ATKMS53L1M_H
#define LIBCA_EM_DRIVER_ATKMS53L1M_H

#include <em_base/datatype.h>

// 外部模式
#define LIBCA_ATK_MS53L1M_PORT_MODE_EXTERN 1
// 动态模式
#define LIBCA_ATK_MS53L1M_PORT_MODE_DYNAMIC 2

#ifndef LIBCA_ATK_MS53L1M_PORT_MODE
#    define LIBCA_ATK_MS53L1M_PORT_MODE LIBCA_ATK_MS53L1M_PORT_MODE_EXTERN
#endif

#ifdef __cplusplus
extern "C" {
#endif

#if (LIBCA_ATK_MS53L1M_PORT_MODE == LIBCA_ATK_MS53L1M_PORT_MODE_EXTERN)

/// @brief UART 初始化
/// @param baudrate 波特率
///
extern void port_atk_ms53l1m_uart_init(u32 baudrate);

/// @brief UART 发送数据
/// @param buf 数据缓冲区
/// @param len 数据长度
///
extern void port_atk_ms53l1m_uart_send(u8* buf, u16 len);

/// @brief 获取 UART 接收到的一帧数据
/// @return 帧数据指针
///
extern u8* port_atk_ms53l1m_uart_rx_get_frame(void);

/// @brief 获取 UART 接收帧的长度
/// @return 帧长度
///
extern u16 port_atk_ms53l1m_uart_rx_get_frame_len(void);

/// @brief 重新开始 UART 接收
///
extern void port_atk_ms53l1m_uart_rx_restart(void);

/// @brief 毫秒延时
/// @param ms 延时时间（ms）
///
extern void port_atk_ms53l1m_delay_ms(u32 ms);

#elif (LIBCA_ATK_MS53L1M_PORT_MODE == LIBCA_ATK_MS53L1M_PORT_MODE_DYNAMIC)

typedef struct atk_ms53l1m_port
{
    // uart初始化函数
    void (*uart_init)(u32 baudrate);
    // uart发送函数
    void (*uart_send)(u8* buf, u16 len);
    // 获取uart接收帧函数
    u8* (*uart_rx_get_frame)(void);
    // 获取uart接收帧长度函数
    u16 (*uart_rx_get_frame_len)(void);
    // uart接收重启函数
    void (*uart_rx_restart)(void);
    // 毫秒延时函数
    void (*delay_ms)(u32 ms);
} atk_ms53l1m_port_t;

void atk_ms53l1m_bind_port(const atk_ms53l1m_port_t* port);
bool atk_ms53l1m_port_is_registered(void);

#else
#    error "Invalid ATK_MS53L1M port mode"
#endif

/* 错误码 */
#define ATK_MS53L1M_OK 0           /* 没有错误 */
#define ATK_MS53L1M_ERR -1         /* 错误 */
#define ATK_MS53L1M_ERR_TIMEOUT -2 /* 超时错误 */
#define ATK_MS53L1M_ERR_FRAME -3   /* 帧错误 */
#define ATK_MS53L1M_ERR_CRC -4     /* CRC校验错误 */
#define ATK_MS53L1M_ERR_OPT -5     /* 操作错误 */

/* ATK-MS53L1M工作模式 */
typedef enum
{
    ATK_MS53L1M_MODE_NORMAL = 0x00, /* Normal模式 */
    ATK_MS53L1M_MODE_MODBUS = 0x01, /* Modbus模式 */
    ATK_MS53L1M_MODE_IIC    = 0x02, /* IIC模式 */
} atk_ms53l1m_mode_t;

/* ATK-MS53L1M对象 */
typedef struct atk_ms53l1m
{
    u16                device_id; /* 设备地址 */
    u32                baudrate;  /* 波特率 */
    atk_ms53l1m_mode_t work_mode; /* 工作模式 */
} atk_ms53l1m_t;

/// @brief       ATK-MS53L1M初始化
/// @param       self: atk_ms53l1m对象
///              baudrate: ATK-MS53L1M UART通讯波特率
///              work_mode: 工作模式
/// @retval      ATK_MS53L1M_OK  : ATK-MS53L1M初始化成功
///              ATK_MS53L1M_ERR: ATK-MS53L1M初始化失败
///
i32 atk_ms53l1m_init(atk_ms53l1m_t* self, u32 baudrate, atk_ms53l1m_mode_t work_mode);

/// @brief       ATK-MS53L1M Normal工作模式获取测量值
/// @param       self: atk_ms53l1m对象
///              dat: 获取到的测量值
/// @retval      ATK_MS53L1M_OK : 获取测量值成功
///              ATK_MS53L1M_ERR: UART未接收到数据，获取测量值失败
///
i32 atk_ms53l1m_normal_get_data(atk_ms53l1m_t* self, u16* dat);

/// @brief       ATK-MS53L1M Modbus工作模式获取测量值
/// @param       self: atk_ms53l1m对象
///              dat: 获取到的测量值
/// @retval      ATK_MS53L1M_OK : 获取测量值成功
///              ATK_MS53L1M_ERR: UART未接收到数据，获取测量值失败
///
i32 atk_ms53l1m_modbus_get_data(atk_ms53l1m_t* self, u16* dat);

#ifdef __cplusplus
}
#endif

#endif   // !LIBCA_EM_DRIVER_ATKMS53L1M_H