#ifndef LIBCA_EM_EIMUI_HANDLER_H
#define LIBCA_EM_EIMUI_HANDLER_H

#include "eimui.h"

// 这些函数将由 Python 脚本自动生成并在 menu_handler.c 中实现
void menu_handler_main(menu_context_t* ctx, menu_t* menu);
void menu_handler_setting(menu_context_t* ctx, menu_t* menu);
void menu_handler_about(menu_context_t* ctx, menu_t* menu);

// 用户手动实现的页面
void menu_handler_custom_ui(menu_context_t* ctx, menu_t* menu);

#endif
