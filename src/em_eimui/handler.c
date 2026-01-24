#include "handler.h"
#include "data.h"
#include <string.h>
#include "eimui.h"
#include "st7735_mocker.h"

// 这个文件里的代码本质上都是由 Python 生成的
// 它通过硬编码的方式绘制每一页，从而彻底省掉结构体查表的开销

static void draw_item(st7735_ops_t* ctx, eimui_t* self, u16 y, const char* label, bool selected) {
    u16 fg = selected ? self->color_bg : self->color_fg;
    u16 bg = selected ? self->color_fg : self->color_bg;
    
    if (selected && ctx->fill_rect) {
        ctx->fill_rect(0, y, self->width, self->font_size, bg);
    }
    
    if (ctx->draw_string) {
        if (selected) {
            ctx->draw_string(4, y, "->", fg, bg);
        }
        // 给起始位置留出 2 个字符的宽度 (假设每个字约 8-12 像素，这里给 24 像素偏移)
        ctx->draw_string(24, y, label, fg, bg);
    }
}

void eimui_handler_main(void* dops, eimui_t* self) {
    // 逻辑处理 (Python 根据配置生成此 switch)
    if (self->event == EIMUI_EVENT_ENTER) {
        switch (self->cursor_pos) {
            case 0: eimui_set_page(self, PAGE_ID_SETTING); break;
            case 1: eimui_set_page(self, PAGE_ID_CUSTOM); break; // 跳转到纯用户绘图页
            case 2: eimui_exit(self); break;
        }
    } else if (self->event == EIMUI_EVENT_UP) {
        if (self->cursor_pos > 0) self->cursor_pos--;
    } else if (self->event == EIMUI_EVENT_DOWN) {
        if (self->cursor_pos < 2) self->cursor_pos++;
    }

    st7735_ops_t* ctx = (st7735_ops_t*)dops;

    // 绘制处理 (Python 根据配置生成这里的渲染代码)
    u16 y = 0;
    if (ctx->draw_string) ctx->draw_string(2, y, "--- MAIN ---", self->color_fg, self->color_bg);
    y += self->font_size;
    
    draw_item(ctx, self, y, "Settings", self->cursor_pos == 0); y += self->font_size;
    draw_item(ctx, self, y, "About",    self->cursor_pos == 1); y += self->font_size;
    draw_item(ctx, self, y, "Exit",     self->cursor_pos == 2); y += self->font_size;
}

void eimui_handler_setting(void* dops, eimui_t* self) {
    if (self->event == EIMUI_EVENT_BACK) {
        eimui_set_page(self, PAGE_ID_MAIN);
        return;
    }
    
    st7735_ops_t* ctx = (st7735_ops_t*)dops;
    // 省略具体的按键逻辑... 
    if (ctx->draw_string) ctx->draw_string(2, 0, "--- SETTINGS ---", self->color_fg, self->color_bg);
    draw_item(ctx, self, 20, "Brightness", self->cursor_pos == 0);
    draw_item(ctx, self, 40, "Volume",     self->cursor_pos == 1);
}

#include <math.h>
void eimui_handler_custom_ui(void* dops, eimui_t* self) {
    if (self->event == EIMUI_EVENT_BACK) {
        eimui_set_page(self, PAGE_ID_MAIN);
        return;
    }

    // 纯手写的绘图逻辑
    static int angle = 0;
    angle = (angle + 2) % 360;

    u16 x = 120 + (u16)(50 * cos(angle * 0.0174f));
    u16 y = 100 + (u16)(50 * sin(angle * 0.0174f));
    
    st7735_ops_t* ctx = (st7735_ops_t*)dops;
    ctx->fill_rect(0, 0, self->width, self->height, 0x1111);
    ctx->fill_rect(x, y, 40, 40, 0xF800);
    
    ctx->draw_string(10, 10, "USER CUSTOM PAGE", 0xFFFF, 0x0000);
    ctx->draw_string(10, 220, "Press ESC to go back", 0x7E0, 0x0000);
}
