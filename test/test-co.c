#include <stdio.h>
#include <stdint.h>

// 定义协程的宏
#define CR_BEGIN static int state = 0; switch(state) { case 0:
#define CR_YIELD(x) do { state = __LINE__; return x; case __LINE__:; } while (0)
#define CR_END }

// 协程1：模拟一个LED闪烁任务
int led_blink_co(void) {
    static uint32_t count = 0;
    CR_BEGIN;
    
    for(;;) {
        printf("LED ON!\n");
        count = 0;
        // 模拟延时，让出CPU 5次
        while(count++ < 5) {
            CR_YIELD(0); // 每次被调用，都yield一次
        }

        printf("LED OFF!\n");
        count = 0;
        // 模拟延时，让出CPU 10次
        while(count++ < 10) {
            CR_YIELD(0);
        }
    }
    
    CR_END;
    return 0;
}

// 协程2：模拟一个按键检测任务
int button_check_co(void) {
    CR_BEGIN;
    
    for(;;) {
        printf("Checking button...\n");
        // 模拟检测到按键按下，需要处理
        if (/* some condition */ 1) { 
            printf("Button PRESSED! Processing...\n");
            // 模拟一个需要分步完成的处理过程
            CR_YIELD(0); // 第一步
            printf("Processing step 2...\n");
            CR_YIELD(0); // 第二步
            printf("Processing step 3... Done!\n");
        }
        CR_YIELD(0); // 每次循环都让出CPU
    }

    CR_END;
    return 0;
}


// 主循环 - 协程调度器
int main() {
    uint32_t i = 0;
    for(i = 0; i < 100; i++) {
        printf("--- Main Loop Tick %d ---\n", i);
        led_blink_co();    // 驱动LED协程
        button_check_co(); // 驱动按键协程
    }
    return 0;
}
