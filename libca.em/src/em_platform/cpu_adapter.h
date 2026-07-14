/// @file cpu_port.h
/// @author canrad (1517807724@qq.com)
/// @brief CPU 架构相关的接口定义 (Porting Layer)
/// @version 0.1
/// @date 2025-12-31
///
/// @copyright Copyright (c) 2025
///
#ifndef LIBCA_ARCH_EM_CPU_PORT_H
#define LIBCA_ARCH_EM_CPU_PORT_H

#if USE_CUSTOM_CPU_ADAPTER
void local_cpu_enter_critical(void);
void local_cpu_exit_critical(void);
#else
// 提供一个默认的空实现，避免未定义
static inline void local_cpu_enter_critical(void) {
    // 默认空实现
}
static inline void local_cpu_exit_critical(void) {
    // 默认空实现
}
#endif

/// @brief 进入临界区 (禁止中断/锁定)
/// @note 用户需根据具体平台实现此宏或函数
#ifndef CPU_ENTER_CRITICAL
#define CPU_ENTER_CRITICAL() local_cpu_enter_critical()
#endif

/// @brief 退出临界区 (恢复中断/解锁)
/// @note 用户需根据具体平台实现此宏或函数
#ifndef CPU_EXIT_CRITICAL
#define CPU_EXIT_CRITICAL() local_cpu_exit_critical()
#endif

#endif // LIBCA_ARCH_EM_CPU_PORT_H
