#define SDL_MAIN_HANDLED
#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#include <stdio.h>
#include <string.h>
#include "eimui.h"
#include "st7735_mocker.h"

static bool g_quit = false;
extern SDL_Window* g_window;
extern SDL_Renderer* g_renderer;
extern TTF_Font* g_font;

int main(int argc, char* argv[]) {
    if (SDL_Init(SDL_INIT_VIDEO) < 0) return -1;
    if (TTF_Init() < 0) return -1;

    g_window = SDL_CreateWindow("MCU Menu Simulator", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, 320, 240, SDL_WINDOW_SHOWN);
    g_renderer = SDL_CreateRenderer(g_window, -1, SDL_RENDERER_ACCELERATED);

    // 加载中文字体以支持中文显示，请确保路径正确
    // Windows 下常用的字体路径: "C:/Windows/Fonts/msyh.ttc" (微软雅黑) 或 "C:/Windows/Fonts/simhei.ttf" (黑体)
    g_font = TTF_OpenFont("C:/Windows/Fonts/msyh.ttc", 16);
    if (!g_font) {
        // 如果找不到微软雅黑，尝试 Arial (仅支持英文)
        g_font = TTF_OpenFont("C:/Windows/Fonts/arial.ttf", 16);
    }

    eimui_t my_menu;
    eimui_init(&my_menu, 320, 240);
    my_menu.should_repaint = 1; // 默认开启重绘测试

    eimui_context_t* ctx = get_st7735_context();
    
    while (!my_menu.should_exit && !g_quit) {
        // 1. 获取输入 (外部采集)
        SDL_Event e;
        while (SDL_PollEvent(&e)) {
            if (e.type == SDL_QUIT) g_quit = true;
            if (e.type == SDL_KEYDOWN) {
                switch (e.key.keysym.sym) {
                    case SDLK_UP:     eimui_input_event(&my_menu, EIMUI_EVENT_UP); break;
                    case SDLK_DOWN:   eimui_input_event(&my_menu, EIMUI_EVENT_DOWN); break;
                    case SDLK_RETURN: eimui_input_event(&my_menu, EIMUI_EVENT_ENTER); break;
                    case SDLK_ESCAPE: eimui_input_event(&my_menu, EIMUI_EVENT_BACK); break;
                }
            }
        }

        // 2. 一切尽在 menu_tick (Master Pulse)
        // 现在 render() 和 frame_control() 已经被“注入”进去由 menu_tick 调用了
        eimui_tick(ctx, &my_menu);
    }

    if (g_font) TTF_CloseFont(g_font);
    SDL_DestroyRenderer(g_renderer);
    SDL_DestroyWindow(g_window);
    TTF_Quit();
    SDL_Quit();
    return 0;
}
