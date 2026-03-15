#include "illume.h"
#include <em_base/debug.h>

#if (LIBCA_ILLUME_PORT_MODE == LIBCA_ILLUME_PORT_MODE_EXTERN)
static const illume_port_t* g_port = &g_illume_port_extern;
#elif (LIBCA_ILLUME_PORT_MODE == LIBCA_ILLUME_PORT_MODE_DYNAMIC)
static const illume_port_t* g_port = NULL;
#else
#error "Invalid ILLUME port mode"
#endif

void illume_bind_port(const illume_port_t* port) {
    g_port = port;
}

bool illume_port_is_registered(void) {
    return g_port != NULL;
}

void illume_init(illume_t* self, void* adc_hdl, u8 adc_ch, void* do_gpio, u16 do_pin, u8 adc_resolution) {
    self->adc_hdl = adc_hdl;
    self->adc_ch  = adc_ch;
    self->do_gpio = do_gpio;
    self->do_pin  = do_pin;
    
    // 计算 ADC 最大值
    if (adc_resolution > 0 && adc_resolution <= 16) {
        self->adc_max = (1UL << adc_resolution) - 1;
    } else {
        // 对于无效分辨率，默认使用12位以防止后续计算出错。
        self->adc_max = 4095;
        debug_print("[illume] Invalid ADC resolution, defaulting to 12-bit.\n");
    }
}

u8 illume_get_percentage(illume_t* self, u8 count) {
    if (count == 0) count = 1;

    u32 sum = 0;
    for (u8 i = 0; i < count; i++) {
        sum += g_port->read_adc(self->adc_hdl, self->adc_ch);
    }

    u16 avg = (u16)(sum / count);

    // 计算百分比：100 * (1 - avg/max)
    // 通常光照越强 ADC 值越低（依赖具体分压电路，此处遵循原实现逻辑）
    if (avg > self->adc_max) avg = self->adc_max;

    f32 ratio = (f32)avg / self->adc_max;
    u8 percentage = (u8)((1.0f - ratio) * 100.0f);

    return percentage;
}

u8 illume_get_do_state(illume_t* self) {
    return g_port->read_pin(self->do_gpio, self->do_pin);
}
