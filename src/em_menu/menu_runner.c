#define SDL_MAIN_HANDLED
#include "menu.h"
#include "menu_router.h"
#include "menu_data.h"
#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#include <stdio.h>
#include <string.h>


// SDL2 适配层
static SDL_Window* g_window = NULL;
static SDL_Renderer* g_renderer = NULL;
static TTF_Font* g_font = NULL;
static bool g_quit = false;

void sdl_draw_string(u16 x, u16 y, const char* str, u16 color_fg, u16 color_bg) {
    if (!g_font) return;

    SDL_Color fg = { (u8)((color_fg >> 11) << 3), (u8)(((color_fg >> 5) & 0x3F) << 2), (u8)((color_fg & 0x1F) << 3), 255 };
    SDL_Color bg = { (u8)((color_bg >> 11) << 3), (u8)(((color_bg >> 5) & 0x3F) << 2), (u8)((color_bg & 0x1F) << 3), 255 };

    // 使用 TTF_RenderText_Shaded 可以同时绘制背景色和前景色
    SDL_Surface* surface = TTF_RenderUTF8_Shaded(g_font, str, fg, bg);
    if (!surface) return;

    SDL_Texture* texture = SDL_CreateTextureFromSurface(g_renderer, surface);
    SDL_Rect dst_rect = { x, y, surface->w, surface->h };
    
    SDL_RenderCopy(g_renderer, texture, NULL, &dst_rect);

    SDL_FreeSurface(surface);
    SDL_DestroyTexture(texture);
}

void sdl_fill_rect(u16 x, u16 y, u16 w, u16 h, u16 color) {
    SDL_Rect rect = { x, y, w, h };
    SDL_SetRenderDrawColor(g_renderer, (color >> 11) << 3, ((color >> 5) & 0x3F) << 2, (color & 0x1F) << 3, 255);
    SDL_RenderFillRect(g_renderer, &rect);
}

void sdl_render(void) {
    SDL_RenderPresent(g_renderer);
    SDL_SetRenderDrawColor(g_renderer, 0, 0, 0, 255);
    SDL_RenderClear(g_renderer);
}

void sdl_frame_control(void) {
    SDL_Delay(16);
}

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

    menu_t my_menu;
    menu_init(&my_menu, 320, 240);
    my_menu.should_repaint = 1; // 默认开启重绘测试

    menu_context_t ctx = {
        .draw_string = sdl_draw_string,
        .fill_rect = sdl_fill_rect,
        .render = sdl_render,
        .frame_control = sdl_frame_control
    };

    while (!my_menu.should_exit && !g_quit) {
        // 1. 获取输入 (外部采集)
        SDL_Event e;
        while (SDL_PollEvent(&e)) {
            if (e.type == SDL_QUIT) g_quit = true;
            if (e.type == SDL_KEYDOWN) {
                switch (e.key.keysym.sym) {
                    case SDLK_UP:     menu_input_event(&my_menu, MENU_EVENT_UP); break;
                    case SDLK_DOWN:   menu_input_event(&my_menu, MENU_EVENT_DOWN); break;
                    case SDLK_RETURN: menu_input_event(&my_menu, MENU_EVENT_ENTER); break;
                    case SDLK_ESCAPE: menu_input_event(&my_menu, MENU_EVENT_BACK); break;
                }
            }
        }

        // 2. 一切尽在 menu_tick (Master Pulse)
        // 现在 render() 和 frame_control() 已经被“注入”进去由 menu_tick 调用了
        menu_tick(&ctx, &my_menu);
    }

    if (g_font) TTF_CloseFont(g_font);
    SDL_DestroyRenderer(g_renderer);
    SDL_DestroyWindow(g_window);
    TTF_Quit();
    SDL_Quit();
    return 0;
}
