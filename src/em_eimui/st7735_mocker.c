#include "st7735_mocker.h"
#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#include <string.h>
#include <stdlib.h>

// SDL2 适配层
SDL_Window* g_window = NULL;
SDL_Renderer* g_renderer = NULL;
TTF_Font* g_font = NULL;

// forward prototypes for ops used in the context
void sdl_draw_string(u16 x, u16 y, const char* str, u16 color_fg, u16 color_bg);
void sdl_fill_rect(u16 x, u16 y, u16 w, u16 h, u16 color);
void sdl_render(void);
void sdl_frame_control(void);

// operations and static context exposed by get_st7735_context
static const st7735_ops_t g_ops = {
    .draw_string = sdl_draw_string,
    .fill_rect = sdl_fill_rect
};

static const eimui_context_t g_ctx = {
    .dops = (void*)&g_ops,
    .render = sdl_render,
    .frame_control = sdl_frame_control
};

eimui_context_t* get_st7735_context(void) {
    return (eimui_context_t*)&g_ctx;
}

// 文本纹理缓存
#define TEXT_CACHE_SIZE 64
typedef struct {
    char* str;
    u16 fg;
    u16 bg;
    SDL_Texture* tex;
    int w;
    int h;
} text_cache_entry_t;

static text_cache_entry_t g_text_cache[TEXT_CACHE_SIZE];

static inline uint8_t from5to8(uint8_t v) { return (v << 3) | (v >> 2); }
static inline uint8_t from6to8(uint8_t v) { return (v << 2) | (v >> 4); }

// 简单线性查找缓存
static int find_cache(const char* str, u16 fg, u16 bg) {
    for (int i = 0; i < TEXT_CACHE_SIZE; i++) {
        if (g_text_cache[i].tex && g_text_cache[i].fg == fg && g_text_cache[i].bg == bg && strcmp(g_text_cache[i].str, str) == 0) {
            return i;
        }
    }
    return -1;
}

static int alloc_cache_slot(void) {
    for (int i = 0; i < TEXT_CACHE_SIZE; i++) {
        if (g_text_cache[i].tex == NULL) return i;
    }
    // 简单替换策略：释放第0个
    int i = 0;
    if (g_text_cache[i].tex) {
        SDL_DestroyTexture(g_text_cache[i].tex);
        free(g_text_cache[i].str);
        g_text_cache[i].tex = NULL;
        g_text_cache[i].str = NULL;
    }
    return i;
}

static void create_cache_entry(int idx, const char* str, u16 fg, u16 bg) {
    if (!g_renderer || !g_font) return;
    SDL_Color fg_col = { from5to8((fg >> 11) & 0x1F), from6to8((fg >> 5) & 0x3F), from5to8(fg & 0x1F), 255 };
    SDL_Color bg_col = { from5to8((bg >> 11) & 0x1F), from6to8((bg >> 5) & 0x3F), from5to8(bg & 0x1F), 255 };
    SDL_Surface* surface = TTF_RenderUTF8_Shaded(g_font, str, fg_col, bg_col);
    if (!surface) return;
    SDL_Texture* tex = SDL_CreateTextureFromSurface(g_renderer, surface);
    if (!tex) {
        SDL_FreeSurface(surface);
        return;
    }
    g_text_cache[idx].str = strdup(str);
    g_text_cache[idx].fg = fg;
    g_text_cache[idx].bg = bg;
    g_text_cache[idx].tex = tex;
    g_text_cache[idx].w = surface->w;
    g_text_cache[idx].h = surface->h;
    SDL_FreeSurface(surface);
}

void sdl_draw_string(u16 x, u16 y, const char* str, u16 color_fg, u16 color_bg) {
    if (!g_font || !g_renderer) return;

    int id = find_cache(str, color_fg, color_bg);
    if (id >= 0) {
        text_cache_entry_t* e = &g_text_cache[id];
        SDL_Rect dst = { (int)x, (int)y, e->w, e->h };
        SDL_RenderCopy(g_renderer, e->tex, NULL, &dst);
        return;
    }

    int slot = alloc_cache_slot();
    create_cache_entry(slot, str, color_fg, color_bg);
    if (g_text_cache[slot].tex) {
        SDL_Rect dst = { (int)x, (int)y, g_text_cache[slot].w, g_text_cache[slot].h };
        SDL_RenderCopy(g_renderer, g_text_cache[slot].tex, NULL, &dst);
    }
}

void sdl_fill_rect(u16 x, u16 y, u16 w, u16 h, u16 color) {
    if (!g_renderer) return;
    int rw = 0, rh = 0;
    SDL_GetRendererOutputSize(g_renderer, &rw, &rh);

    int lx = x, ly = y, lw = w, lh = h;
    if (lx < 0) { lw += lx; lx = 0; }
    if (ly < 0) { lh += ly; ly = 0; }
    if (lx + lw > rw) lw = rw - lx;
    if (ly + lh > rh) lh = rh - ly;
    if (lw <= 0 || lh <= 0) return;

    SDL_Rect rect = { lx, ly, lw, lh };
    SDL_SetRenderDrawColor(g_renderer, from5to8((color >> 11) & 0x1F), from6to8((color >> 5) & 0x3F), from5to8(color & 0x1F), 255);
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

void sdl_mocker_cleanup(void) {
    for (int i = 0; i < TEXT_CACHE_SIZE; i++) {
        if (g_text_cache[i].tex) {
            SDL_DestroyTexture(g_text_cache[i].tex);
            g_text_cache[i].tex = NULL;
        }
        if (g_text_cache[i].str) {
            free(g_text_cache[i].str);
            g_text_cache[i].str = NULL;
        }
    }
}
