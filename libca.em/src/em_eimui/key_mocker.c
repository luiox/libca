#include "key_mocker.h"
#include "eimui.h"

static eimui_t* s_ui = NULL;

void key_mocker_init(eimui_t* ui)
{
    s_ui = ui;
}

void key_mocker_handle_event(const SDL_Event* e)
{
    if (!s_ui || !e)
        return;

    if (e->type == SDL_KEYDOWN) {
        switch (e->key.keysym.sym) {
        case SDLK_UP: eimui_input_event(s_ui, EIMUI_EVENT_UP); break;
        case SDLK_DOWN: eimui_input_event(s_ui, EIMUI_EVENT_DOWN); break;
        case SDLK_LEFT: eimui_input_event(s_ui, EIMUI_EVENT_BACK); break;
        case SDLK_RIGHT: eimui_input_event(s_ui, EIMUI_EVENT_ENTER); break;
        case SDLK_RETURN: eimui_input_event(s_ui, EIMUI_EVENT_ENTER); break;
        case SDLK_ESCAPE: eimui_input_event(s_ui, EIMUI_EVENT_BACK); break;
        }
    }
}
