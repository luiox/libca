#include "w25qxx.h"
#include <em_base/compiler_compat.h>

CA_WEAK void port_w25qxx_write_pin(void* gpio_port, u16 pin, u8 value)
{
    unused_param(gpio_port);
    unused_param(pin);
    unused_param(value);
}

CA_WEAK void port_w25qxx_spi_transmit(void* hspi, u8* data, usize size, u32 timeout)
{
    unused_param(hspi);
    unused_param(data);
    unused_param(size);
    unused_param(timeout);
}

CA_WEAK void port_w25qxx_spi_receive(void* hspi, u8* data, usize size, u32 timeout)
{
    unused_param(hspi);
    unused_param(data);
    unused_param(size);
    unused_param(timeout);
}

CA_WEAK void port_w25qxx_spi_transmit_receive(void* hspi, u8* tx_data, u8* rx_data, usize size, u32 timeout)
{
    unused_param(hspi);
    unused_param(tx_data);
    unused_param(rx_data);
    unused_param(size);
    unused_param(timeout);
}
