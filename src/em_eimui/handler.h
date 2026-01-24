#ifndef LIBCA_EM_EIMUI_HANDLER_H
#define LIBCA_EM_EIMUI_HANDLER_H

#include "eimui.h"

// 这些函数将由 Python 脚本自动生成并在 menu_handler.c 中实现
void eimui_handler_main(void* dops, eimui_t* self);
void eimui_handler_setting(void* dops, eimui_t* self);
void eimui_handler_about(void* dops, eimui_t* self);

// 用户手动实现的页面
void eimui_handler_custom_ui(void* dops, eimui_t* self);

#endif
