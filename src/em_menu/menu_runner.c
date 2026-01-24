#define SDL_MAIN_HANDLED
#include "menu.h"
#include "menu_router.h"
#include "menu_data.h"
#include <SDL2/SDL.h>
#include <stdio.h>
#include <stdbool.h>

// SDL2 全局变量
static SDL_Window* g_window = NULL;
static SDL_Renderer* g_renderer = NULL;
static bool g_quit = false;

// --- 适配层实现 ---

void sdl_draw_string(u16 x, u16 y, const char* str, u16 color_fg, u16 color_bg) {
    // 模拟：在 SDL 窗口里画一个矩形代表文字区域 (真实开发请使用 SDL_ttf)
    SDL_Rect rect = { x, y, (int)strlen(str) * 8, 16 };
    u8 r = (color_fg >> 11) << 3;
    u8 g = ((color_fg >> 5) & 0x3F) << 2;
    u8 b = (color_fg & 0x1F) << 3;

    // 先画背景颜色
    u8 br = (color_bg >> 11) << 3;
    u8 bg_g = ((color_bg >> 5) & 0x3F) << 2;
    u8 bg_b = (color_bg & 0x1F) << 3;
    SDL_SetRenderDrawColor(g_renderer, br, bg_g, bg_b, 255);
    SDL_RenderFillRect(g_renderer, &rect);

    // 再画一个框代表文字
    SDL_SetRenderDrawColor(g_renderer, r, g, b, 255);
    SDL_RenderDrawRect(g_renderer, &rect);
}

void sdl_fill_rect(u16 x, u16 y, u16 w, u16 h, u16 color) {
    SDL_Rect rect = { x, y, w, h };
    u8 r = (color >> 11) << 3;
    u8 g = ((color >> 5) & 0x3F) << 2;
    u8 b = (color & 0x1F) << 3;
    SDL_SetRenderDrawColor(g_renderer, r, g, b, 255);
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

// --- 纯用户绘制页面示例 (手写代码) ---

void my_custom_ui_handler(menu_context_t* ctx, menu_t* menu) {
    // 处理退出自定义界面的逻辑
    if (menu->event == MENU_EVENT_BACK) {
        menu_set_page(menu, PAGE_ID_MAIN);
        return;
    }

    // 这里是纯手写的绘图逻辑，完全不依赖于菜单的 cursor_pos 逻辑
    static int angle = 0;
    angle = (angle + 2) % 360;

    // 画一个动起来的色块
    u16 x = 120 + (u16)(50 * cos(angle * 0.0174f));
    u16 y = 100 + (u16)(50 * sin(angle * 0.0174f));
    
    ctx->fill_rect(0, 0, menu->width, menu->height, 0x1111); // 深灰色背景
    ctx->fill_rect(x, y, 40, 40, 0xF800);                   // 红色动块
    
    ctx->draw_string(10, 10, "USER CUSTOM PAGE", 0xFFFF, 0x0000);
    ctx->draw_string(10, 220, "Press ESC to go back", 0x7E0, 0x0000);
}

// --- 为了演示，我们在运行时手动把自定义页面挂载到路由 ---
// 在真实项目中，这可以在 menu_router.c 的 g_routes 数组里静态初始化
extern menu_page_handler_t g_user_custom_handler; 

int main(int argc, char* argv[]) {
    if (SDL_Init(SDL_INIT_VIDEO) < 0) return -1;

    g_window = SDL_CreateWindow("Simulator - Pure UI + Menu", 
                               SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, 
                               320, 240, SDL_WINDOW_SHOWN);
    g_renderer = SDL_CreateRenderer(g_window, -1, SDL_RENDERER_ACCELERATED);

    menu_t my_menu;
    menu_init(&my_menu, 320, 240);

    menu_context_t ctx = {
        .draw_string = sdl_draw_string,
        .fill_rect = sdl_fill_rect,
        .render = sdl_render,
        .frame_control = sdl_frame_control
    };

    // 核心循环：模拟主流程管理
    while (!my_menu.should_exit && !g_quit) {
        // 1. 获取输入
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

        // 2. 驱动菜单/流程核心逻辑
        // 如果是自定义页面，可以使用特别的逻辑分发
        if (my_menu.current_page == PAGE_ID_CUSTOM) {
            my_custom_ui_handler(&ctx, &my_menu);
        } else {
            menu_tick(&ctx, &my_menu);
        }
        
        // 3. 提交绘图
        sdl_render();
        sdl_frame_control();
    }

    SDL_DestroyRenderer(g_renderer);
    SDL_DestroyWindow(g_window);
    SDL_Quit();
    return 0;
}
