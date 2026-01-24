#include "st7735_mocker.h"
#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>

// SDL2 适配层
SDL_Window* g_window = NULL;
SDL_Renderer* g_renderer = NULL;
TTF_Font* g_font = NULL;


void sdl_draw_string(u16 x, u16 y, const char* str, u16 color_fg, u16 color_bg) ;
void sdl_fill_rect(u16 x, u16 y, u16 w, u16 h, u16 color) ;
void sdl_render(void);
void sdl_frame_control(void);

const st7735_ops_t g_ops = {
    .draw_string = sdl_draw_string,
    .fill_rect = sdl_fill_rect
};

const eimui_context_t g_ctx = {
        .dops = (void*)&g_ops,
        .render = sdl_render,
        .frame_control = sdl_frame_control
};

eimui_context_t* get_st7735_context(void)
{
    return (void*)&g_ctx;
}

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