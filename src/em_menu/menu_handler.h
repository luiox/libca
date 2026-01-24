#ifndef LIBCA_EM_MENU_MENU_HANDLER_H
#define LIBCA_EM_MENU_MENU_HANDLER_H

#include "menu.h"

// 这些函数将由 Python 脚本自动生成并在 menu_handler.c 中实现
void menu_handler_main(menu_context_t* ctx, menu_t* menu);
void menu_handler_setting(menu_context_t* ctx, menu_t* menu);
void menu_handler_about(menu_context_t* ctx, menu_t* menu);

#endif
