#include "key_util.h"

///////////////////////////////////////////////////////////////////////////////

// 需要移植的部分

void key_read_pin(key_t* key)
{
    // 读取按键的GPIO引脚状态，设置对应的引脚状态
    // 例如hal库的实现可以如下。
    // key->key_state = HAL_GPIO_ReadPin(key->gpioPort, key->gpioPin);
    key->key_state = KEY_STATE_PRESS;
}

///////////////////////////////////////////////////////////////////////////////

// 按键信息
key_t keys[REAL_KEYS_SIZE] = {0};

// 按键判断状态
// 按键没有按下的状态
#define KEY_JUDGE_STATE_NORMAL 0
// 按键第一次按下
#define KEY_JUDGE_STATE_FIRST_PRESS 1
// 按键滤波状态，等待确定按下
#define KEY_JUDGE_STATE_FILTER 2
// 按键确定按下状态
#define KEY_JUDGE_STATE_PRESS 3

void key_scan_all(void)
{
    for (u32 i = 0; i < array_size(keys); i++) {
        key_read_pin(&keys[i]);
    }
    for (u32 i = 0; i < array_size(keys); i++) {
        // 按键状态判断
        switch (keys[i].judge_state) {
        case KEY_JUDGE_STATE_NORMAL:
            if (keys[i].key_state == KEY_STATE_PRESS) {
                keys[i].judge_state = KEY_JUDGE_STATE_FIRST_PRESS;
                keys[i].time       = 0;
            }
            break;
        case KEY_JUDGE_STATE_FIRST_PRESS:
            if (keys[i].key_state == KEY_STATE_PRESS) {
                keys[i].judge_state = KEY_JUDGE_STATE_FILTER;
            }
            else {
                keys[i].judge_state = KEY_JUDGE_STATE_NORMAL;
            }
            break;
        case KEY_JUDGE_STATE_FILTER:
            // 滤波状态
            if(keys[i].time < KEY_ELIMIT_DITCHING_TICK){
                keys[i].time++;
            }else{
                keys[i].judge_state = KEY_JUDGE_STATE_PRESS;
                // 重置时间，因为后面是存的是按下时间
                keys[i].time = 0;
            }
            break;
        case KEY_JUDGE_STATE_PRESS:
            if (keys[i].key_state == KEY_STATE_PRESS) {
                keys[i].time++;
            }
            else {
                if (keys[i].time < KEY_MAX_SHORT_TICK) {
                    keys[i].short_flag = 1;
                }
                else if (keys[i].time > KEY_MIN_LONG_TICK) {
                    keys[i].long_flag = 1;
                }else{
                    keys[i].normal_flag = 1;
                }
                keys[i].judge_state = 0;
            }
            break;
        }
    }
}

key_t* key_get(s8 id)
{
    return &keys[id];
}
