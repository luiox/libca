/**
 * @file cpu_port.h
 * @author canrad (1517807724@qq.com)
 * @brief CPU 架构相关的接口定义 (Porting Layer)
 * @version 0.1
 * @date 2025-12-31
 * 
 * @copyright Copyright (c) 2025
 * 
 */
#ifndef EM_CPU_PORT_H
#define EM_CPU_PORT_H

#include "datatype.h"

#if USE_CUSTOM_CPU_ADAPTER
#include "cpu_adapter.h"
#endif

/**
 * @brief 进入临界区 (禁止中断/锁定)
 * @note 用户需根据具体平台实现此宏或函数
 */
#ifndef EM_CPU_ENTER_CRITICAL
#define EM_CPU_ENTER_CRITICAL()
#endif

/**
 * @brief 退出临界区 (恢复中断/解锁)
 * @note 用户需根据具体平台实现此宏或函数
 */
#ifndef EM_CPU_EXIT_CRITICAL
#define EM_CPU_EXIT_CRITICAL()
#endif

#endif // EM_CPU_PORT_H
