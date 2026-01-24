#include "menu.h"
#include "menu_router.h"
#include <string.h>

void menu_init(menu_t* menu, u16 w, u16 h) {
    memset(menu, 0, sizeof(menu_t));
    menu->width = w;
    menu->height = h;
    menu->color_fg = 0xFFFF; // Default White
    menu->color_bg = 0x0000; // Default Black
    menu->font_size = 16;
    menu->current_page = 0;
}

void menu_tick(menu_context_t* ctx, menu_t* menu) {
    if (menu->should_exit) return;

    // 1. 根据当前页面ID路由到对应的处理函数，处理输入事件和绘制页面
    menu_route_handler(ctx, menu);

    // 2. handler处理完后清空事件
    menu->event = MENU_EVENT_NONE;

    // 3. 渲染到屏幕
    if(menu->should_repaint){
        ctx->render();
    }

    // 4. 控制帧率
    ctx->frame_control();
}

void menu_set_page(menu_t* menu, page_t page_id) {
    if (menu->current_page != page_id) {
        menu->last_page = menu->current_page;
        menu->current_page = page_id;
        menu->cursor_pos = 0;
        menu->scroll_offset = 0;
    }
}

void menu_exit(menu_t* menu) {
    menu->should_exit = 1;
}

void menu_input_event(menu_t* menu, menu_event_t event) {
    menu->event = event;
}
