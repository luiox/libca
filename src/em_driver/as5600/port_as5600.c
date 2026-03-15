#include "as5600.h"
#include <em_base/compiler_compat.h>

/**
 * @brief 外部隐式注入的 port 函数表，适配层可提供强符号覆盖此定义
 */
CA_WEAK const as5600_port_t g_as5600_port_extern = {0};
