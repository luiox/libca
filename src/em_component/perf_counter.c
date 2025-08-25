// #include "perf_counter.h"
// // 包含您项目中的MSPM0G3507设备头文件，以便访问SysTick寄存器
// // 这个文件名可能类似 ti_msp_dl_config.h 或者 mspm0g3507.h
// #include "ti_msp_dl_config.h" 

// // SysTick最大计数值 (24位)
// #define SYSTICK_RELOAD_VAL 0x00FFFFFF
// uint32_t SystemCoreClock = CPUCLK_FREQ;
// // 用于存储SysTick溢出次数的全局变量，必须是volatile
// static volatile uint64_t g_systick_overflow_count = 0;

// /**
//  * @brief SysTick中断服务函数 (直接重写)
//  * @note  每次SysTick从0x00FFFFFF减到0时，会触发此中断。
//  * 我们在这里累加溢出次数。
//  */
// void SysTick_Handler(void) {
//     g_systick_overflow_count++;
// }

// void init_cycle_counter(uint8_t enable) {
//     if (!enable) {
//         SysTick->CTRL = 0; // 禁用SysTick
//         return;
//     }

//     // 配置SysTick作为自由运行的24位向下计数器
//     SysTick->LOAD = SYSTICK_RELOAD_VAL; // 设置重装载值
//     SysTick->VAL = 0;                   // 清空当前计数值
//     SysTick->CTRL = SysTick_CTRL_CLKSOURCE_Msk | // 使用处理器时钟
//                     SysTick_CTRL_TICKINT_Msk   | // 开启中断
//                     SysTick_CTRL_ENABLE_Msk;    // 使能SysTick
// }

// uint64_t get_system_ticks(void) {
//     uint32_t primask = __get_PRIMASK(); // 保存当前中断状态
//     __disable_irq(); // 禁止中断，防止在读取计数值时发生溢出中断

//     // 读取当前的SysTick计数值和溢出次数
//     uint32_t current_val = SysTick->VAL;
//     uint64_t overflow_count = g_systick_overflow_count;

//     // 检查在读取VAL之后，SysTick是否已经溢出 (COUNTFLAG位会被置1)
//     // 这是一个关键步骤，用于防止在读取VAL和overflow_count之间发生中断而导致的数据不一致
//     if ((SysTick->CTRL & SysTick_CTRL_COUNTFLAG_Msk) == SysTick_CTRL_COUNTFLAG_Msk) {
//         // 如果发生了溢出，说明我们读取的overflow_count是旧的
//         // 此时需要重新读取计数值，并使用递增后的溢出计数值
//         current_val = SysTick->VAL;
//         overflow_count++;
//         // 在CCS的调试器中，当单步执行此代码块时，COUNTFLAG可能不会被硬件自动清除。
//         // 读取它即会清除该位，所以上面的if条件判断已经完成了清除操作。
//     }

//     __set_PRIMASK(primask); // 恢复之前的中断状态

//     // 计算总的ticks
//     // 总ticks = 溢出次数 * 单次计数的周期数 + (最大值 - 当前值)
//     return (overflow_count * (uint64_t)SYSTICK_RELOAD_VAL) + (uint64_t)(SYSTICK_RELOAD_VAL - current_val);
// }

// uint64_t get_system_us(void) {
//     // 确保SystemCoreClock已正确设置！
//     if (SystemCoreClock == 0) return 0;
    
//     // 使用64位整数运算防止溢出
//     return (get_system_ticks() * 1000000) / SystemCoreClock;
// }

// uint64_t get_system_ms(void) {
//     // 确保SystemCoreClock已正确设置！
//     if (SystemCoreClock == 0) return 0;

//     return get_system_ticks() / (SystemCoreClock / 1000);
// }

// void delay_us(uint32_t us) {
//     uint64_t start_us = get_system_us();
//     while ((get_system_us() - start_us) < us) {
//         // 忙等待
//     }
// }

// void delay_ms(uint32_t ms) {
//     delay_us(ms * 1000);
// }