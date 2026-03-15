#include "w25qxx.h"
#include <em_base/compiler_compat.h>

/**
 * @brief 外部隐式注入的弱符号接口实现
 */

CA_WEAK void port_w25qxx_write_pin(void* gpio_port, u16 pin, u8 value)
{
    (void)gpio_port;
    (void)pin;
    (void)value;
}

CA_WEAK void port_w25qxx_spi_transmit(void* hspi, u8* data, usize size, u32 timeout)
{
    (void)hspi;
    (void)data;
    (void)size;
    (void)timeout;
}

CA_WEAK void port_w25qxx_spi_receive(void* hspi, u8* data, usize size, u32 timeout)
{
    (void)hspi;
    (void)data;
    (void)size;
    (void)timeout;
}

CA_WEAK void port_w25qxx_spi_transmit_receive(void* hspi, u8* tx_data, u8* rx_data, usize size, u32 timeout)
{
    (void)hspi;
    (void)tx_data;
    (void)rx_data;
    (void)size;
    (void)timeout;
}
