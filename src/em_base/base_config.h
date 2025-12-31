/**
 * @file base_config.h
 * @author canrad (1517807724@qq.com)
 * @brief 这个文件包含了base内所有的配置内容
 *   文件结构为：平台相关配置、模块配置、通用内容的定义
 *
 * @version 0.1
 * @date 2024-08-16
 * 
 */
#ifndef LIBCA_EM_BASE_CONFIG_H
#define LIBCA_EM_BASE_CONFIG_H

#define HAS_INT64 1

///////////////////////////////////////////////////////////////////////////////

// debug模块的配置
// 使用串口打印信息，debug模块需要定义这个宏为1
#define USE_DEBUG_MODE 1
// 使用调试断言，需要定义这个宏为1
#define USE_DEBUG_ASSERT 1
// 使用参数检查，需要定义这个宏为1
#define USE_PARAM_CHECK 1

#endif   // !LIBCA_EM_BASE_CONFIG_H
