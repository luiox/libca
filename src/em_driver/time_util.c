#include "time_util.h"

// 需要定义CPU的频率，CPUCLK_FREQ由ti_msp_dl_config.h定义
#define MCU_CLOCK_FREQ CPUCLK_FREQ

// 阻塞延时函数
void delay_us(uint32_t us)
{
    // HAL_Delay(us/1000);
}

void delay_ms(uint32_t ms)
{
    // HAL_Delay(ms);
}

// 非阻塞延时函数
void delay_us_noblock(uint32_t us)
{
    // vTaskDelay(pdMS_TO_TICKS(us / 1000));
}

void delay_ms_noblock(uint32_t ms)
{
    // vTaskDelay(pdMS_TO_TICKS(1000));
}

u32  g_tick;
void ms_timer_irq_handler(void)
{
    g_tick++;
}

u32 time_get_current_tick(void)
{
    return g_tick;
}
