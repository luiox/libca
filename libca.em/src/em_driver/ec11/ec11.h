/// @file ec11.h
/// @author Canrad
/// @brief EC11 旋转编码器驱动
/// 参考文档：https://wiki.lckfb.com/zh-hans/tkx/tkx-stm32f407vxt6/module/sensor/ec11.html
/// @version 0.1
/// @date 2026-01-23
/// @update 0.2 添加extern外部依赖注入模式
///
/// @copyright Copyright (c) 2026
///
#ifndef LIBCA_EM_DRIVER_EC11_H
#define LIBCA_EM_DRIVER_EC11_H

#include <em_base/datatype.h>

// 外部模式
#define LIBCA_EC11_PORT_MODE_EXTERN 1
// 动态模式
#define LIBCA_EC11_PORT_MODE_DYNAMIC 2

#ifndef LIBCA_EC11_PORT_MODE
#    define LIBCA_EC11_PORT_MODE LIBCA_EC11_PORT_MODE_EXTERN
#endif

#ifdef __cplusplus
extern "C" {
#endif

#if (LIBCA_EC11_PORT_MODE == LIBCA_EC11_PORT_MODE_EXTERN)
/// @brief 读引脚电平
/// @param gpio GPIO端口
/// @param pin 引脚号
/// @return 当前电平值
extern u8 port_ec11_read_pin(void* gpio, u16 pin);

#elif (LIBCA_EC11_PORT_MODE == LIBCA_EC11_PORT_MODE_DYNAMIC)
typedef struct ec11_port
{
    u8 (*read_pin)(void* gpio, u16 pin);   // 读引脚电平
} ec11_port_t;
void ec11_bind_port(const ec11_port_t* port);
bool ec11_port_is_registered(void);

#else
#    error "Invalid EC11 port mode"
#endif

/// @brief 旋转方向枚举
typedef enum ec11_rotation_enum
{
    EC11_ROTATION_NONE  = 0,
    EC11_ROTATION_LEFT  = 1,
    EC11_ROTATION_RIGHT = 2
} ec11_rotation;

/// @brief EC11 对象结构体
typedef struct ec11
{
    void* clk_gpio;
    u16   clk_pin;
    void* dt_gpio;
    u16   dt_pin;
    void* sw_gpio;
    u16   sw_pin;

    u8 last_clk_state;
    u8 last_dt_state;
    u8 last_sw_state;

    // sw是高电平还是低电平时候是按下
    u8 sw_when_down_state;

    i32           rotation_count;   // 累计旋转计数值
    ec11_rotation last_item;        // 上一次扫描探测到的旋转方向
} ec11_t;

/// @brief 初始化 EC11 对象
///
/// @param self 对象指针
/// @param clk_gpio CLK 引脚的 GPIO 句柄
/// @param clk_pin CLK 引脚编号
/// @param dt_gpio DT 引脚的 GPIO 句柄
/// @param dt_pin DT 引脚编号
/// @param sw_gpio SW 引脚的 GPIO 句柄
/// @param sw_pin SW 引脚编号
/// @param sw_when_down_state SW 按下时的电平（0 或 1）
void ec11_init(ec11_t* self, void* clk_gpio, u16 clk_pin, void* dt_gpio, u16 dt_pin, void* sw_gpio,
               u16 sw_pin, u8 sw_when_down_state);

/// @brief 扫描 EC11 状态
///
/// @param self 对象指针
/// @return ec11_rotation 探测到的旋转方向
ec11_rotation ec11_scan(ec11_t* self);

/// @brief 获取旋转计数值
///
/// @param self 对象指针
/// @return i32 计数值
i32 ec11_get_count(ec11_t* self);

/// @brief 重置旋转计数值
///
/// @param self 对象指针
void ec11_reset_count(ec11_t* self);

/// @brief 获取按键是否按下
///
/// @param self 对象指针
/// @return bool true 为按下
/// @note 内部不包含延时消抖，调用者根据业务需求可以自行添加 delay (如 100ms) 以确保状态稳定
bool ec11_is_sw_down(ec11_t* self);

/// @brief 获取上一次探测到的旋转方向
///
/// @param self 对象指针
/// @return ec11_rotation 上一次探测到的旋转方向
ec11_rotation ec11_get_last_rotation(ec11_t* self);

#ifdef __cplusplus
}
#endif

#endif   // !LIBCA_EM_DRIVER_EC11_H
