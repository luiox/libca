#include "menu_router.h"
#include "menu_data.h"
#include "menu_handler.h"

void menu_page_none_handler(menu_context_t* ctx, struct menu* menu)
{

}

void menu_route_handler(menu_context_t* ctx, struct menu* menu)
{
    switch (menu->current_page) {
    case PAGE_ID_MAIN: menu_handler_main(ctx, menu); break;
    case PAGE_ID_SETTING:  menu_handler_setting(ctx, menu); break;
    case PAGE_ID_CUSTOM: menu_handler_custom_ui(ctx, menu); break;
    default: menu_page_none_handler(ctx, menu);
    }
}
