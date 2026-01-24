#include "menu_handler.h"
#include "menu_data.h"
#include <string.h>

// 这个文件里的代码本质上都是由 Python 生成的
// 它通过硬编码的方式绘制每一页，从而彻底省掉结构体查表的开销

static void draw_item(menu_context_t* ctx, menu_t* menu, u16 y, const char* label, bool selected) {
    u16 fg = selected ? menu->color_bg : menu->color_fg;
    u16 bg = selected ? menu->color_fg : menu->color_bg;
    
    if (selected && ctx->fill_rect) {
        ctx->fill_rect(0, y, menu->width, menu->font_size, bg);
    }
    
    if (ctx->draw_string) {
        if (selected) {
            ctx->draw_string(4, y, "->", fg, bg);
        }
        // 给起始位置留出 2 个字符的宽度 (假设每个字约 8-12 像素，这里给 24 像素偏移)
        ctx->draw_string(24, y, label, fg, bg);
    }
}

void menu_handler_main(menu_context_t* ctx, menu_t* menu) {
    // 逻辑处理 (Python 根据配置生成此 switch)
    if (menu->event == MENU_EVENT_ENTER) {
        switch (menu->cursor_pos) {
            case 0: menu_set_page(menu, PAGE_ID_SETTING); break;
            case 1: menu_set_page(menu, PAGE_ID_CUSTOM); break; // 跳转到纯用户绘图页
            case 2: menu_exit(menu); break;
        }
    } else if (menu->event == MENU_EVENT_UP) {
        if (menu->cursor_pos > 0) menu->cursor_pos--;
    } else if (menu->event == MENU_EVENT_DOWN) {
        if (menu->cursor_pos < 2) menu->cursor_pos++;
    }

    // 绘制处理 (Python 根据配置生成这里的渲染代码)
    u16 y = 0;
    if (ctx->draw_string) ctx->draw_string(2, y, "--- MAIN ---", menu->color_fg, menu->color_bg);
    y += menu->font_size;
    
    draw_item(ctx, menu, y, "Settings", menu->cursor_pos == 0); y += menu->font_size;
    draw_item(ctx, menu, y, "About",    menu->cursor_pos == 1); y += menu->font_size;
    draw_item(ctx, menu, y, "Exit",     menu->cursor_pos == 2); y += menu->font_size;
}

void menu_handler_setting(menu_context_t* ctx, menu_t* menu) {
    if (menu->event == MENU_EVENT_BACK) {
        menu_set_page(menu, PAGE_ID_MAIN);
        return;
    }
    
    // 省略具体的按键逻辑... 
    if (ctx->draw_string) ctx->draw_string(2, 0, "--- SETTINGS ---", menu->color_fg, menu->color_bg);
    draw_item(ctx, menu, 20, "Brightness", menu->cursor_pos == 0);
    draw_item(ctx, menu, 40, "Volume",     menu->cursor_pos == 1);
}

#include <math.h>
void menu_handler_custom_ui(menu_context_t* ctx, menu_t* menu) {
    if (menu->event == MENU_EVENT_BACK) {
        menu_set_page(menu, PAGE_ID_MAIN);
        return;
    }

    // 纯手写的绘图逻辑
    static int angle = 0;
    angle = (angle + 2) % 360;

    u16 x = 120 + (u16)(50 * cos(angle * 0.0174f));
    u16 y = 100 + (u16)(50 * sin(angle * 0.0174f));
    
    ctx->fill_rect(0, 0, menu->width, menu->height, 0x1111);
    ctx->fill_rect(x, y, 40, 40, 0xF800);
    
    ctx->draw_string(10, 10, "USER CUSTOM PAGE", 0xFFFF, 0x0000);
    ctx->draw_string(10, 220, "Press ESC to go back", 0x7E0, 0x0000);
}
