#include "key.h"

static key_port_t* g_key_port = NULL;

void key_bind_port(key_port_t* port)
{
    g_key_port = port;
}

bool key_port_is_registered(void)
{
    return g_key_port != NULL;
}

void key_init(key_t* self)
{
    // nothing to do
}

// 按键判断状态
// 按键没有按下的状态
#define KEY_JUDGE_STATE_NORMAL 0
// 按键第一次按下
#define KEY_JUDGE_STATE_FIRST_PRESS 1
// 按键滤波状态，等待确定按下
#define KEY_JUDGE_STATE_FILTER 2
// 按键确定按下状态
#define KEY_JUDGE_STATE_PRESS 3

void key_scan_all(key_t* keys, usize keys_size)
{
    for (u32 i = 0; i < keys_size; i++) {
        keys[i].key_state = g_key_port->read_pin(keys[i].gpio, keys[i].pin);
    }
    for (u32 i = 0; i < keys_size; i++) {
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
