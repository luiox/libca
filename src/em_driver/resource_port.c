#include "resource_port.h"
#include "resource_util.h"

// 需要移植到不同单片机平台的部分，暂时保持空
u8 gpio_read_pin(resource_id_t id)
{
    return 0;
}

void gpio_write_pin(resource_id_t id, u8 value)
{

}

void gpio_toggle_pin(resource_id_t id){

}