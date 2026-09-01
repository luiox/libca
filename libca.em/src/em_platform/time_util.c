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


static time_get_fn_t g_ms_provider    = NULL;
static time_get_fn_t g_us_provider    = NULL;
static volatile u32  g_manual_ms_tick = 0;

void time_set_ms_provider(time_get_fn_t provider)
{
    g_ms_provider = provider;
}

void time_update_tick_ms(u32 ms_delta)
{
    g_manual_ms_tick += ms_delta;
}

void time_set_us_provider(time_get_fn_t provider)
{
    g_us_provider = provider;
}

u32 time_get_ms(void)
{
    if (g_ms_provider) {
        return g_ms_provider();
    }
    return g_manual_ms_tick;
}

u32 time_get_us(void)
{
    if (g_us_provider) {
        return g_us_provider();
    }
    // 如果没有 us 提供者，返回0
    return 0;
}
