#ifndef ST7735_MOCKER_H
#define ST7735_MOCKER_H
#include "../em_base/datatype.h"
#include "eimui.h"

typedef struct st7735_ops{
    // x和y是起始的坐标
    void (*draw_string)(u16 x, u16 y, const char* str, u16 color_fg, u16 color_bg);
    // 填充矩形
    void (*fill_rect)(u16 x, u16 y, u16 w, u16 h, u16 color);
}st7735_ops_t;

eimui_context_t* get_st7735_context(void);

// 释放内部资源 (纹理缓存等)
void sdl_mocker_cleanup(void);

#endif 
