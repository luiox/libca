#include "eimui.h"
#include <string.h>

void eimui_init(eimui_t* eimui, u16 w, u16 h) {
    memset(eimui, 0, sizeof(eimui_t));
    eimui->width = w;
    eimui->height = h;
    eimui->color_fg = 0xFFFF; // Default White
    eimui->color_bg = 0x0000; // Default Black
    eimui->font_size = 16;
    eimui->current_page = 0;
}

void eimui_tick(eimui_context_t* ctx, eimui_t* self) {
    if (self->should_exit) return;

    // 1. 根据当前页面ID路由到对应的处理函数，处理输入事件和绘制页面
    eimui_route_handler(ctx->dops, self);

    // 2. handler处理完后清空事件
    self->event = EIMUI_EVENT_NONE;

    // 3. 渲染到屏幕
    if(self->should_repaint){
        ctx->render();
    }

    // 4. 控制帧率
    ctx->frame_control();
}

void eimui_set_page(eimui_t* eimui, page_t page_id) {
    if (eimui->current_page != page_id) {
        eimui->last_page = eimui->current_page;
        eimui->current_page = page_id;
        eimui->cursor_pos = 0;
        eimui->scroll_offset = 0;
    }
}

void eimui_exit(eimui_t* eimui) {
    eimui->should_exit = 1;
}

void eimui_input_event(eimui_t* eimui, eimui_event_t event) {
    eimui->event = event;
}
