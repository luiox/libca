// // perf_counter.h

// #ifndef PERF_COUNTER_H_
// #define PERF_COUNTER_H_

// #include <stdint.h>

// #ifdef __cplusplus
// extern "C" {
// #endif

// /**
//  * @brief 系统核心时钟频率变量 (重要!)
//  * @note  这个变量必须由您的工程根据实际配置进行定义和赋值。
//  * 对于MSPM0G3507，这通常在TI提供的启动文件或SysConfig生成的
//  * 文件中已经定义。您需要确保它的值是正确的CPU核心频率。
//  * 例如: uint32_t SystemCoreClock = 32000000; // 32MHz
//  */
// extern uint32_t SystemCoreClock;

// /**
//  * @brief 初始化周期计数器
//  * @param enable 总是传入1来使能
//  */
// void init_cycle_counter(uint8_t enable);

// /**
//  * @brief 获取自启动以来的总CPU时钟周期数 (Ticks)
//  * @return 64位的总周期数
//  */
// uint64_t get_system_ticks(void);

// /**
//  * @brief 获取自启动以来的总微秒数 (us)
//  * @return 64位的总微秒数
//  */
// uint64_t get_system_us(void);

// /**
//  * @brief 获取自启动以来的总毫秒数 (ms)
//  * @return 64位的总毫秒数
//  */
// uint64_t get_system_ms(void);

// /**
//  * @brief 阻塞式延时 (微秒)
//  * @param us 需要延时的微秒数
//  */
// void delay_us(uint32_t us);

// /**
//  * @brief 阻塞式延时 (毫秒)
//  * @param ms 需要延时的毫秒数
//  */
// void delay_ms(uint32_t ms);

// #ifdef __cplusplus
// }
// #endif

// #endif /* PERF_COUNTER_H_ */