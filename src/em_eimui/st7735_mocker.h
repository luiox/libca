#ifndef ST7735_MOCKER_H
#define ST7735_MOCKER_H
#include <em_base/datatype.h>
#include "eimui.h"

typedef struct st7735_ops{
    // 兼容原有简化接口
    void (*draw_string)(u16 x, u16 y, const char* str, u16 color_fg, u16 color_bg);
    void (*fill_rect)(u16 x, u16 y, u16 w, u16 h, u16 color);

    // 完整 ST7735 风格接口（由用户请求）
    void (*display_on)(void);
    void (*display_off)(void);
    void (*clear)(u16 color);
    void (*draw_point)(u16 x, u16 y, u16 color);
    void (*fill)(u16 sx, u16 sy, u16 ex, u16 ey, u16 color);
    void (*draw_line)(u16 x1, u16 y1, u16 x2, u16 y2, u16 color);
    void (*draw_rectangle)(u16 x1, u16 y1, u16 x2, u16 y2, u16 color);
    void (*draw_circle)(u16 x0, u16 y0, u8 r, u16 color);
    void (*draw_full_circle)(u16 Xpos, u16 Ypos, u16 Radius, u16 Color);
    void (*draw_char)(u16 x, u16 y, u8 num, u8 size, u8 mode, u16 pen_color, u16 back_color);
    void (*draw_string_ex)(u16 x, u16 y, u16 width, u16 height, u8 size, u8* p, u16 pen_color, u16 back_color);
}st7735_ops_t;

eimui_context_t* get_st7735_context(void);

// 释放内部资源 (纹理缓存等)
void sdl_mocker_cleanup(void);

// 兼容 ST7735 样式的函数实现
void st7735s_display_on(void);
void st7735s_display_off(void);
void st7735s_clear(u16 color);
void st7735s_draw_point(u16 x, u16 y, u16 color);
void st7735s_fill(u16 sx, u16 sy, u16 ex, u16 ey, u16 color);
void st7735s_draw_line(u16 x1, u16 y1, u16 x2, u16 y2, u16 color);
void st7735s_draw_rectangle(u16 x1, u16 y1, u16 x2, u16 y2, u16 color);
void st7735s_draw_circle(u16 x0, u16 y0, u8 r, u16 color);
void st7735s_draw_full_circle(u16 Xpos, u16 Ypos, u16 Radius, u16 Color);
void st7735s_draw_char(u16 x, u16 y, u8 num, u8 size,
                       u8 mode, u16 pen_color, u16 back_color);
void st7735s_draw_string(u16 x, u16 y, u16 width,
                         u16 height, u8 size, u8* p,
                         u16 pen_color, u16 back_color);

#endif 
