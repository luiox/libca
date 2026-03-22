#include "st7735_mocker.h"
#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#include <string.h>
#include <stdlib.h>
#include <math.h>

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
// forward declaration of g_ops (defined later with full interface)
static const st7735_ops_t g_ops;

static const eimui_context_t g_ctx = {
    .dops = (void*)&g_ops, // g_ops defined later with full interface
    .render = sdl_render,
    .frame_control = sdl_frame_control
};

eimui_context_t* get_st7735_context(void) {
    return (eimui_context_t*)&g_ctx;
}

// 文本纹理缓存
#define TEXT_CACHE_SIZE 64
#define DEFAULT_FONT_SIZE 16

typedef struct {
    char* str;
    u8 size;        // 字号（模拟字段，单位与 TTF 逻辑对应）
    u16 fg;
    u16 bg;
    SDL_Texture* tex;
    int w;
    int h;
} text_cache_entry_t;

static text_cache_entry_t g_text_cache[TEXT_CACHE_SIZE];

static inline uint8_t from5to8(uint8_t v) { return (v << 3) | (v >> 2); }
static inline uint8_t from6to8(uint8_t v) { return (v << 2) | (v >> 4); }

// 简单线性查找缓存（包含 size）
static int find_cache(const char* str, u8 size, u16 fg, u16 bg) {
    for (int i = 0; i < TEXT_CACHE_SIZE; i++) {
        if (g_text_cache[i].tex && g_text_cache[i].fg == fg && g_text_cache[i].bg == bg && g_text_cache[i].size == size && strcmp(g_text_cache[i].str, str) == 0) {
            return i;
        }
    }
    return -1;
}

static int alloc_cache_slot(void) {
    for (int i = 0; i < TEXT_CACHE_SIZE; i++) {
        if (g_text_cache[i].tex == NULL) return i;
    }
    // Simple round-robin replacement strategy
    static int s_evict_idx = 0;
    int i = s_evict_idx;
    s_evict_idx = (s_evict_idx + 1) % TEXT_CACHE_SIZE;

    if (g_text_cache[i].tex) {
        SDL_DestroyTexture(g_text_cache[i].tex);
        free(g_text_cache[i].str);
        g_text_cache[i].tex = NULL;
        g_text_cache[i].str = NULL;
    }
    return i;
}

static void create_cache_entry(int idx, const char* str, u8 size, u16 fg, u16 bg) {
    if (!g_renderer) return;

    // 尝试使用传入 size 的字体，如果没有则回退到 g_font（以路径重用有开销，这里做简单处理）
    TTF_Font* font_to_use = g_font;
    SDL_Color fg_col = { from5to8((fg >> 11) & 0x1F), from6to8((fg >> 5) & 0x3F), from5to8(fg & 0x1F), 255 };
    SDL_Color bg_col = { from5to8((bg >> 11) & 0x1F), from6to8((bg >> 5) & 0x3F), from5to8(bg & 0x1F), 255 };

    // 如果主字体大小和请求 size 不一致，尝试用 TTF_RenderUTF8_Shaded 仍然有效（字体会被缩放），所以不额外打开文件
    SDL_Surface* surface = TTF_RenderUTF8_Shaded(font_to_use, str, fg_col, bg_col);
    if (!surface) return;
    SDL_Texture* tex = SDL_CreateTextureFromSurface(g_renderer, surface);
    if (!tex) {
        SDL_FreeSurface(surface);
        return;
    }
    g_text_cache[idx].str = strdup(str);
    g_text_cache[idx].fg = fg;
    g_text_cache[idx].bg = bg;
    g_text_cache[idx].size = size;
    g_text_cache[idx].tex = tex;
    g_text_cache[idx].w = surface->w;
    g_text_cache[idx].h = surface->h;
    SDL_FreeSurface(surface);
}

// Basic color helper for renderer
static void set_render_color_from565(u16 color) {
    SDL_SetRenderDrawColor(g_renderer, from5to8((color >> 11) & 0x1F), from6to8((color >> 5) & 0x3F), from5to8(color & 0x1F), 255);
}

// Display on/off state
static int g_display_on = 1;

void st7735s_display_on(void) { g_display_on = 1; }
void st7735s_display_off(void) { g_display_on = 0; SDL_SetRenderDrawColor(g_renderer, 0, 0, 0, 255); SDL_RenderClear(g_renderer); }

void st7735s_clear(u16 color) {
    if (!g_renderer) return;
    int rw = 0, rh = 0;
    SDL_GetRendererOutputSize(g_renderer, &rw, &rh);
    set_render_color_from565(color);
    SDL_Rect rect = {0, 0, rw, rh};
    SDL_RenderFillRect(g_renderer, &rect);
}

void st7735s_draw_point(u16 x, u16 y, u16 color) {
    if (!g_renderer || !g_display_on) return;
    set_render_color_from565(color);
    SDL_RenderDrawPoint(g_renderer, x, y);
}

void st7735s_fill(u16 sx, u16 sy, u16 ex, u16 ey, u16 color) {
    if (!g_renderer || !g_display_on) return;
    int lx = sx < ex ? sx : ex;
    int ly = sy < ey ? sy : ey;
    int lw = (ex >= sx) ? (ex - sx + 1) : (sx - ex + 1);
    int lh = (ey >= sy) ? (ey - sy + 1) : (sy - ey + 1);
    SDL_Rect rect = { lx, ly, lw, lh };
    set_render_color_from565(color);
    SDL_RenderFillRect(g_renderer, &rect);
}

void st7735s_draw_line(u16 x1, u16 y1, u16 x2, u16 y2, u16 color) {
    if (!g_renderer || !g_display_on) return;
    set_render_color_from565(color);
    SDL_RenderDrawLine(g_renderer, x1, y1, x2, y2);
}

void st7735s_draw_rectangle(u16 x1, u16 y1, u16 x2, u16 y2, u16 color) {
    if (!g_renderer || !g_display_on) return;
    int lx = x1 < x2 ? x1 : x2;
    int ly = y1 < y2 ? y1 : y2;
    int lw = (x2 >= x1) ? (x2 - x1 + 1) : (x1 - x2 + 1);
    int lh = (y2 >= y1) ? (y2 - y1 + 1) : (y1 - y2 + 1);
    SDL_Rect rect = { lx, ly, lw, lh };
    set_render_color_from565(color);
    SDL_RenderDrawRect(g_renderer, &rect);
}

void st7735s_draw_circle(u16 x0, u16 y0, u8 r, u16 color) {
    if (!g_renderer || !g_display_on) return;
    set_render_color_from565(color);
    int f = 1 - r;
    int ddF_x = 1;
    int ddF_y = -2 * r;
    int x = 0;
    int y = r;

    SDL_RenderDrawPoint(g_renderer, x0, y0 + r);
    SDL_RenderDrawPoint(g_renderer, x0, y0 - r);
    SDL_RenderDrawPoint(g_renderer, x0 + r, y0);
    SDL_RenderDrawPoint(g_renderer, x0 - r, y0);

    while (x < y) {
        if (f >= 0) {
            y--;
            ddF_y += 2;
            f += ddF_y;
        }
        x++;
        ddF_x += 2;
        f += ddF_x;

        SDL_RenderDrawPoint(g_renderer, x0 + x, y0 + y);
        SDL_RenderDrawPoint(g_renderer, x0 - x, y0 + y);
        SDL_RenderDrawPoint(g_renderer, x0 + x, y0 - y);
        SDL_RenderDrawPoint(g_renderer, x0 - x, y0 - y);
        SDL_RenderDrawPoint(g_renderer, x0 + y, y0 + x);
        SDL_RenderDrawPoint(g_renderer, x0 - y, y0 + x);
        SDL_RenderDrawPoint(g_renderer, x0 + y, y0 - x);
        SDL_RenderDrawPoint(g_renderer, x0 - y, y0 - x);
    }
}

void st7735s_draw_full_circle(u16 Xpos, u16 Ypos, u16 Radius, u16 Color) {
    if (!g_renderer || !g_display_on) return;
    set_render_color_from565(Color);
    // filled circle by scanning lines
    for (int y = -((int)Radius); y <= (int)Radius; y++) {
        int dx = (int)(sqrt((double)(Radius * Radius - y * y)) + 0.5);
        SDL_RenderDrawLine(g_renderer, Xpos - dx, Ypos + y, Xpos + dx, Ypos + y);
    }
}

void st7735s_draw_char(u16 x, u16 y, u8 num, u8 size, u8 mode, u16 pen_color, u16 back_color) {
    if (!g_renderer || !g_font || !g_display_on) return;
    char buf[5] = {0}; // enough for one utf8 char
    buf[0] = (char)num;
    // reuse string cache: treat as string with given size
    int id = find_cache(buf, size, pen_color, back_color);
    if (id >= 0) {
        text_cache_entry_t* e = &g_text_cache[id];
        SDL_Rect dst = { (int)x, (int)y, e->w, e->h };
        SDL_RenderCopy(g_renderer, e->tex, NULL, &dst);
        return;
    }
    int slot = alloc_cache_slot();
    create_cache_entry(slot, buf, size, pen_color, back_color);
    if (g_text_cache[slot].tex) {
        SDL_Rect dst = { (int)x, (int)y, g_text_cache[slot].w, g_text_cache[slot].h };
        SDL_RenderCopy(g_renderer, g_text_cache[slot].tex, NULL, &dst);
    }
}

void st7735s_draw_string(u16 x, u16 y, u16 width, u16 height, u8 size, u8* p, u16 pen_color, u16 back_color) {
    if (!g_renderer || !p || !g_display_on) return;
    const char* s = (const char*)p;
    int id = find_cache(s, size, pen_color, back_color);
    if (id >= 0) {
        text_cache_entry_t* e = &g_text_cache[id];
        int w = e->w < width ? e->w : width;
        int h = e->h < height ? e->h : height;
        SDL_Rect dst = { (int)x, (int)y, w, h };
        SDL_RenderCopy(g_renderer, e->tex, NULL, &dst);
        return;
    }
    int slot = alloc_cache_slot();
    create_cache_entry(slot, s, size, pen_color, back_color);
    if (g_text_cache[slot].tex) {
        text_cache_entry_t* e = &g_text_cache[slot];
        int w = e->w < width ? e->w : width;
        int h = e->h < height ? e->h : height;
        SDL_Rect dst = { (int)x, (int)y, w, h };
        SDL_RenderCopy(g_renderer, e->tex, NULL, &dst);
    }
}

// wire new functions into g_ops
static const st7735_ops_t g_ops = {
    .draw_string = sdl_draw_string,
    .fill_rect = sdl_fill_rect,
    .display_on = st7735s_display_on,
    .display_off = st7735s_display_off,
    .clear = st7735s_clear,
    .draw_point = st7735s_draw_point,
    .fill = st7735s_fill,
    .draw_line = st7735s_draw_line,
    .draw_rectangle = st7735s_draw_rectangle,
    .draw_circle = st7735s_draw_circle,
    .draw_full_circle = st7735s_draw_full_circle,
    .draw_char = st7735s_draw_char,
    .draw_string_ex = st7735s_draw_string
};


void sdl_draw_string(u16 x, u16 y, const char* str, u16 color_fg, u16 color_bg) {
    if (!g_font || !g_renderer) return;

    // 使用默认字号缓存（兼容旧简化接口）
    const u8 size = DEFAULT_FONT_SIZE;

    int id = find_cache(str, size, color_fg, color_bg);
    if (id >= 0) {
        text_cache_entry_t* e = &g_text_cache[id];
        SDL_Rect dst = { (int)x, (int)y, e->w, e->h };
        SDL_RenderCopy(g_renderer, e->tex, NULL, &dst);
        return;
    }

    int slot = alloc_cache_slot();
    create_cache_entry(slot, str, size, color_fg, color_bg);
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
