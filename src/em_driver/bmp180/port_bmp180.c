#include "bmp180.h"
#include <em_base/compiler_compat.h>

/**
 * @brief 外部隐式注入的 port 函数表，适配层可提供强符号覆盖此定义
 */
CA_WEAK const bmp180_port_t g_bmp180_port_extern = {0};
