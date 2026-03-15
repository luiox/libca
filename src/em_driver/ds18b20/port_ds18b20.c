#include "ds18b20.h"
#include <em_base/compiler_compat.h>

/**
 * @brief 外部隐式注入的 port 函数表，适配层可提供强符号覆盖此定义
 */
CA_WEAK const ds18b20_port_t g_ds18b20_port_extern = {0};
