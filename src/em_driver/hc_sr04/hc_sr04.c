#include "hc_sr04.h"

#if (LIBCA_HC_SR04_PORT_MODE == LIBCA_HC_SR04_PORT_MODE_EXTERN)
static const hc_sr04_port_t* g_hc_sr04_port = &g_hc_sr04_port_extern;
#elif (LIBCA_HC_SR04_PORT_MODE == LIBCA_HC_SR04_PORT_MODE_DYNAMIC)
static const hc_sr04_port_t* g_hc_sr04_port = NULL;
#else
#error "Invalid HC_SR04 port mode"
#endif

void hc_sr04_bind_port(const hc_sr04_port_t* port)
{
    g_hc_sr04_port = port;
}

bool hc_sr04_port_is_registered(void)
{
    return g_hc_sr04_port != NULL;
}

// 简化访问宏
#define HC_WRITE_PIN(self, v)   g_hc_sr04_port->write_pin((self)->trig_port, (self)->trig_pin, (v))
#define HC_READ_PIN(self)       g_hc_sr04_port->read_pin((self)->echo_port, (self)->echo_pin)
#define HC_DELAY_US(us)        g_hc_sr04_port->delay_us(us)
#define HC_TIM_SET(self, v)     g_hc_sr04_port->tim_set_counter((self)->tim, (v))
#define HC_TIM_START(self)      g_hc_sr04_port->tim_start((self)->tim)
#define HC_TIM_STOP(self)       g_hc_sr04_port->tim_stop((self)->tim)
#define HC_TIM_GET(self)        g_hc_sr04_port->tim_get_counter((self)->tim)
#define HC_MUTEX_PEND()        do { if (g_hc_sr04_port && g_hc_sr04_port->mutex_pend) g_hc_sr04_port->mutex_pend(); } while(0)
#define HC_MUTEX_POST()        do { if (g_hc_sr04_port && g_hc_sr04_port->mutex_post) g_hc_sr04_port->mutex_post(); } while(0) 

void hc_sr04_init(hc_sr04_t* self, void* trig_port, u16 trig_pin, void* echo_port, u16 echo_pin, void* tim)
{
    self->trig_port = trig_port;
    self->trig_pin  = trig_pin;
    self->echo_port = echo_port;
    self->echo_pin  = echo_pin;
    self->tim       = tim;
    self->distance  = 0.0;
} 

/**
 * 发射一次触发脉冲（至少 10 us）
 */
static void hc_sr04_start_trig(hc_sr04_t* self)
{
    HC_WRITE_PIN(self, 1);
    HC_DELAY_US(10);
    HC_WRITE_PIN(self, 0);
} 

/**
 * 发起测量并返回错误码（HC_SR04_OK 或 错误码）
 * 注意：定时器须配置为微秒计数单位
 */
i32 hc_sr04_measure(hc_sr04_t* self)
{
    if (!g_hc_sr04_port) {
        return HC_SR04_ERR_PORT_NOT_REGISTERED;
    }
    if (!self) {
        return HC_SR04_ERR_INVALID_PARAM;
    }

    const u32 timeout_us = 38000; // 超时时间（微秒），约等于最大测距 ~6.5m
    u32 t;

    HC_MUTEX_PEND();

    // 触发
    hc_sr04_start_trig(self);

    // 重置定时器计数
    HC_TIM_SET(self, 0);

    // 等待 echo 上升（带超时）
    t = timeout_us;
    while (HC_READ_PIN(self) == 0) {
        if (t == 0) {
            HC_MUTEX_POST();
            return HC_SR04_ERR_TIMEOUT;
        }
        HC_DELAY_US(1);
        t--;
    }

    // 启动计时
    HC_TIM_START(self);

    // 等待 echo 下降（带超时）
    t = timeout_us;
    while (HC_READ_PIN(self) == 1) {
        if (t == 0) {
            HC_TIM_STOP(self);
            HC_MUTEX_POST();
            return HC_SR04_ERR_TIMEOUT;
        }
        HC_DELAY_US(1);
        t--;
    }

    // 停止计时并读取计数（单位：us）
    HC_TIM_STOP(self);
    u32 tick_us = HC_TIM_GET(self);

    // 计算距离（cm）: distance = (time_s * sound_speed / 2) * 100
    self->distance = ((double)tick_us / 1000000.0) * 340.0 / 2.0 * 100.0;

    HC_MUTEX_POST();

    return HC_SR04_OK;
}
