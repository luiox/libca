#ifndef EM_EIMUI_KEY_MOCKER_H
#define EM_EIMUI_KEY_MOCKER_H

#include <SDL2/SDL.h>
#include "eimui.h"

void key_mocker_init(eimui_t* ui);
void key_mocker_handle_event(const SDL_Event* e);

#endif // EM_EIMUI_KEY_MOCKER_H
