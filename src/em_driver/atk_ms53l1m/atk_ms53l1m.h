/**
 * @file atkms53l1m.h
 * @author canrad (1517807724@qq.com)
 * @brief ATK-MS53L1M模块驱动代码
 * @version 0.2
 * @date 2026-02-03
 *
 * @copyright Copyright (c) 2026
 *
 */
#ifndef LIBCA_EM_DRIVER_ATKMS53L1M_H
#define LIBCA_EM_DRIVER_ATKMS53L1M_H

#include <em_base/datatype.h>

// 外部模式
#define LIBCA_ATK_MS53L1M_PORT_MODE_EXTERN 1
// 动态模式
#define LIBCA_ATK_MS53L1M_PORT_MODE_DYNAMIC 2

#ifndef LIBCA_ATK_MS53L1M_PORT_MODE
#define LIBCA_ATK_MS53L1M_PORT_MODE LIBCA_ATK_MS53L1M_PORT_MODE_EXTERN
#endif

#ifdef __cplusplus
extern "C" {
#endif

/* port */
typedef struct atk_ms53l1m_port
{
    void (*uart_init)(u32 baudrate);
    void (*uart_send)(u8* buf, u16 len);
    u8* (*uart_rx_get_frame)(void);
    u16 (*uart_rx_get_frame_len)(void);
    void (*uart_rx_restart)(void);
    void (*delay_ms)(u32 ms);
} atk_ms53l1m_port_t;

/**
 * @brief 外部隐式注入的 port 函数表（由 port_atk_ms53l1m.c 提供）
 */
extern const atk_ms53l1m_port_t g_atk_ms53l1m_port_extern;


/**
 * @brief       绑定port
 * @param       port: port接口
 */
void atk_ms53l1m_bind_port(const atk_ms53l1m_port_t* port);

/**
 * @brief       检查port是否已注册
 * @retval      true: 已注册
 *              false: 未注册
 */
bool atk_ms53l1m_port_is_registered(void);

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

/* 错误码 */
#define ATK_MS53L1M_OK 0           /* 没有错误 */
#define ATK_MS53L1M_ERR -1         /* 错误 */
#define ATK_MS53L1M_ERR_TIMEOUT -2 /* 超时错误 */
#define ATK_MS53L1M_ERR_FRAME -3   /* 帧错误 */
#define ATK_MS53L1M_ERR_CRC -4     /* CRC校验错误 */
#define ATK_MS53L1M_ERR_OPT -5     /* 操作错误 */

/**
 * @brief       ATK-MS53L1M初始化
 * @param       self: atk_ms53l1m对象
 *              baudrate: ATK-MS53L1M UART通讯波特率
 *              work_mode: 工作模式
 * @retval      ATK_MS53L1M_OK  : ATK-MS53L1M初始化成功
 *              ATK_MS53L1M_ERR: ATK-MS53L1M初始化失败
 */
i32 atk_ms53l1m_init(atk_ms53l1m_t* self, u32 baudrate, atk_ms53l1m_mode_t work_mode);

/**
 * @brief       ATK-MS53L1M Normal工作模式获取测量值
 * @param       self: atk_ms53l1m对象
 *              dat: 获取到的测量值
 * @retval      ATK_MS53L1M_OK : 获取测量值成功
 *              ATK_MS53L1M_ERR: UART未接收到数据，获取测量值失败
 */
i32 atk_ms53l1m_normal_get_data(atk_ms53l1m_t* self, u16* dat);

/**
 * @brief       ATK-MS53L1M Modbus工作模式获取测量值
 * @param       self: atk_ms53l1m对象
 *              dat: 获取到的测量值
 * @retval      ATK_MS53L1M_OK : 获取测量值成功
 *              ATK_MS53L1M_ERR: UART未接收到数据，获取测量值失败
 */
i32 atk_ms53l1m_modbus_get_data(atk_ms53l1m_t* self, u16* dat);

#ifdef __cplusplus
}
#endif

#endif   // !LIBCA_EM_DRIVER_ATKMS53L1M_H
