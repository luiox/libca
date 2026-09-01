#include "eimui.h"
#include <stdbool.h>

void eimui_init(eimui_t* self, u16 w, u16 h)
{
    self->width  = w;
    self->height = h;

    self->color_fg  = 0xFFFF;   // Default White
    self->color_bg  = 0x0000;   // Default Black
    self->font_size = 16;

    self->current_page = 0;
    self->last_page    = 0;

    self->cursor_pos    = 0;
    self->scroll_offset = 0;

    self->should_exit    = false;
    self->should_repaint = true;

    self->event = EIMUI_EVENT_NONE;
}

void eimui_tick(eimui_context_t* ctx, eimui_t* self)
{
    if (self->should_exit)
        return;

    // 1. 根据当前页面ID路由到对应的处理函数，处理输入事件和绘制页面
    eimui_route_handler(ctx->dops, self);

    // 2. handler处理完后清空事件
    self->event = EIMUI_EVENT_NONE;

    // 3. 渲染到屏幕
    if (self->should_repaint) {
        ctx->render();
        self->should_repaint = false;
    }

    // 4. 控制帧率
    ctx->frame_control();
}

void eimui_set_page(eimui_t* self, page_t page_id)
{
    if (self->current_page != page_id) {
        self->last_page     = self->current_page;
        self->current_page  = page_id;
        self->cursor_pos    = 0;
        self->scroll_offset = 0;
        eimui_repaint(self);
    }
}

void eimui_exit(eimui_t* self)
{
    self->should_exit = true;
}

void eimui_repaint(eimui_t* self)
{
    self->should_repaint = true;
}

void eimui_input_event(eimui_t* self, eimui_event_t event)
{
    self->event = event;
}
