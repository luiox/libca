/**
 * @file ir_track.h
 * @author canrad (1517807724@qq.com)
 * @brief 反射式寻迹的驱动
 * @version 0.1
 * @date 2026-01-23
 * 
 * @copyright Copyright (c) 2026
 * 
 */
#ifndef LIBCA_EM_DRIVER_IR_TRACK_H
#define LIBCA_EM_DRIVER_IR_TRACK_H

#include <em_base/datatype.h>

// 外部模式
#define LIBCA_IR_TRACK_PORT_MODE_EXTERN 1
// 动态模式
#define LIBCA_IR_TRACK_PORT_MODE_DYNAMIC 2

#ifndef LIBCA_IR_TRACK_PORT_MODE
#define LIBCA_IR_TRACK_PORT_MODE LIBCA_IR_TRACK_PORT_MODE_EXTERN
#endif

typedef struct ir_track_port
{
	/**
	 * @brief 读取 gpio 引脚电平
	 *
	 * @param gpio gpio 的地址
	 * @param pin gpio 的引脚
	 * @return u8 1 为高电平，0 为低电平
	 */
	u8 (*read_pin)(void* gpio, u16 pin);
} ir_track_port_t;

#if (LIBCA_IR_TRACK_PORT_MODE == LIBCA_IR_TRACK_PORT_MODE_EXTERN)
extern u8 port_ir_track_read_pin(void* gpio, u16 pin);

#elif (LIBCA_IR_TRACK_PORT_MODE == LIBCA_IR_TRACK_PORT_MODE_DYNAMIC)

/**
 * @brief 显式模式下绑定硬件接口（动态注入）
 */
void ir_track_bind_port(const ir_track_port_t* port);
bool ir_track_port_is_registered(void);

#else
#error "Invalid IR_TRACK port mode"
#endif

typedef struct ir_track
{
	void* gpio;
	u16   pin;
} ir_track_t;

void ir_track_init(ir_track_t* self, void* gpio, u16 pin);
u8   ir_track_get_value(ir_track_t* self);

#endif // !LIBCA_EM_DRIVER_IR_TRACK_H
