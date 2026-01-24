#include "handler.h"
#include "data.h"
#include "eimui.h"
#include "st7735_mocker.h"
#include <stdio.h>
#include <stdint.h>

// 简单的文本项绘制辅助
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
        ctx->draw_string(24, y, label, fg, bg);
    }
}

// MAIN page: Basic / Advance
void eimui_handler_main(void* dops, eimui_t* self) {
    if (self->event == EIMUI_EVENT_ENTER) {
        if (self->cursor_pos == 0) {
            eimui_set_page(self, PAGE_ID_SETTING); // Basic
        } else {
            eimui_set_page(self, PAGE_ID_GAME); // Advance
        }
        self->should_repaint = true;
    } else if (self->event == EIMUI_EVENT_UP) {
        if (self->cursor_pos > 0) self->cursor_pos--;
        self->should_repaint = true;
    } else if (self->event == EIMUI_EVENT_DOWN) {
        if (self->cursor_pos < 1) self->cursor_pos++;
        self->should_repaint = true;
    }

    st7735_ops_t* ctx = (st7735_ops_t*)dops;
    u16 y = 0;
    if (ctx->draw_string) ctx->draw_string(10, y, "MAIN MENU", self->color_fg, self->color_bg);
    y += self->font_size;
    draw_item(ctx, self, y, "Basic", self->cursor_pos == 0); y += self->font_size;
    draw_item(ctx, self, y, "Advance", self->cursor_pos == 1); y += self->font_size;
}

// BASIC page: Task1 / Task2
void eimui_handler_setting(void* dops, eimui_t* self) {
    if (self->event == EIMUI_EVENT_BACK) {
        eimui_set_page(self, PAGE_ID_MAIN);
        self->should_repaint = true;
        return;
    }

    if (self->event == EIMUI_EVENT_ENTER) {
        // Enter selects task
        int task_id = (self->cursor_pos == 0) ? 1 : 2;
        self->user_data = (void*)(intptr_t)task_id;
        eimui_set_page(self, PAGE_ID_CUSTOM);
        self->should_repaint = true;
        return;
    } else if (self->event == EIMUI_EVENT_UP) {
        if (self->cursor_pos > 0) self->cursor_pos--;
        self->should_repaint = true;
    } else if (self->event == EIMUI_EVENT_DOWN) {
        if (self->cursor_pos < 1) self->cursor_pos++;
        self->should_repaint = true;
    }

    st7735_ops_t* ctx = (st7735_ops_t*)dops;
    u16 y = 0;
    if (ctx->draw_string) ctx->draw_string(10, y, "BASIC MENU", self->color_fg, self->color_bg);
    y += self->font_size;
    draw_item(ctx, self, y, "Task 1", self->cursor_pos == 0); y += self->font_size;
    draw_item(ctx, self, y, "Task 2", self->cursor_pos == 1); y += self->font_size;
}

// ADVANCE page: Task3 / Task4
void eimui_handler_about(void* dops, eimui_t* self) {
    if (self->event == EIMUI_EVENT_BACK) {
        eimui_set_page(self, PAGE_ID_MAIN);
        self->should_repaint = true;
        return;
    }

    if (self->event == EIMUI_EVENT_ENTER) {
        int task_id = (self->cursor_pos == 0) ? 3 : 4;
        self->user_data = (void*)(intptr_t)task_id;
        eimui_set_page(self, PAGE_ID_CUSTOM);
        self->should_repaint = true;
        return;
    } else if (self->event == EIMUI_EVENT_UP) {
        if (self->cursor_pos > 0) self->cursor_pos--;
        self->should_repaint = true;
    } else if (self->event == EIMUI_EVENT_DOWN) {
        if (self->cursor_pos < 1) self->cursor_pos++;
        self->should_repaint = true;
    }

    st7735_ops_t* ctx = (st7735_ops_t*)dops;
    u16 y = 0;
    if (ctx->draw_string) ctx->draw_string(10, y, "ADVANCE MENU", self->color_fg, self->color_bg);
    y += self->font_size;
    draw_item(ctx, self, y, "Task 3", self->cursor_pos == 0); y += self->font_size;
    draw_item(ctx, self, y, "Task 4", self->cursor_pos == 1); y += self->font_size;
}

// CUSTOM page: show selected task and allow Back to return
void eimui_handler_custom_ui(void* dops, eimui_t* self) {
    st7735_ops_t* ctx = (st7735_ops_t*)dops;
    int task_id = (int)(intptr_t)self->user_data;

    if (self->event == EIMUI_EVENT_BACK) {
        eimui_set_page(self, self->last_page);
        self->should_repaint = true;
        return;
    }

    if (ctx->fill_rect) ctx->fill_rect(0, 0, self->width, self->height, self->color_bg);

    char buf[32];
    snprintf(buf, sizeof(buf), "TASK %d", task_id);
    if (ctx->draw_string) ctx->draw_string(10, 10, buf, self->color_fg, self->color_bg);

    if (ctx->draw_string) ctx->draw_string(10, self->height - 20, "Press LEFT to go back", self->color_fg, self->color_bg);
}
