/**
 * @file mq_x.c
 * @author GitHub Copilot
 * @brief MQ 系列气体传感器驱动实现
 * @version 0.1
 * @date 2026-01-23
 * 
 * @copyright Copyright (c) 2026
 * 
 */

#include "mq_x.h"
#include "../em_base/debug.h"

static const mqx_port_t* g_port = NULL;

void mqx_bind_port(const mqx_port_t* port) {
    g_port = port;
}

bool mqx_port_is_registered(void) {
    return g_port != NULL;
}

void mqx_init(mqx_t* self, void* adc_hdl, u8 adc_ch, void* do_gpio, u16 do_pin, u8 adc_resolution, u8 samples) {
    self->adc_hdl = adc_hdl;
    self->adc_ch  = adc_ch;
    self->do_gpio = do_gpio;
    self->do_pin  = do_pin;
    
    // 计算 ADC 最大值
    self->adc_max = (1 << adc_resolution) - 1;
    self->samples = (samples == 0) ? 1 : samples;
}

u16 mqx_get_adc(mqx_t* self) {
    if (!g_port || !g_port->read_adc) {
        debug_print("[mqx] error: port not registered\n");
        return 0;
    }

    u32 sum = 0;
    for (u8 i = 0; i < self->samples; i++) {
        sum += g_port->read_adc(self->adc_hdl, self->adc_ch);
    }

    return (u16)(sum / self->samples);
}

u8 mqx_get_percentage(mqx_t* self) {
    u16 avg = mqx_get_adc(self);

    if (avg > self->adc_max) avg = self->adc_max;

    // 计算百分比：100 * (avg/max)
    // 气体浓度越高，AO 输出电压通常越高
    f32 ratio = (f32)avg / self->adc_max;
    u8 percentage = (u8)(ratio * 100.0f);

    return percentage;
}

u8 mqx_get_do_state(mqx_t* self) {
    if (!g_port || !g_port->read_pin) {
        debug_print("[mqx] error: port not registered\n");
        return 0;
    }

    return g_port->read_pin(self->do_gpio, self->do_pin);
}
