#include "eimui.h"
#include "data.h"
#include "handler.h"

void eimui_page_none_handler(void* dops, eimui_t* self) {}

void eimui_route_handler(void* dops, eimui_t* self)
{
    switch (self->current_page) {
    case PAGE_ID_MAIN: eimui_handler_main(dops, self); break;
    case PAGE_ID_BASIC: eimui_handler_basic(dops, self); break;
    case PAGE_ID_ADVANCE: eimui_handler_advance(dops, self); break;   // Advance page
    case PAGE_ID_CUSTOM: eimui_handler_custom_ui(dops, self); break;
    default: eimui_page_none_handler(dops, self);
    }
}
