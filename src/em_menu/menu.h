/**
 * @file menu.h
 * @author canrad (1517807724@qq.com)
 * @brief 数据驱动的MCU下的菜单系统
 * 保证对RAM的使用尽可能小，无动画，支持子菜单、翻页，选项行为
 * @version 0.1
 * @date 2026-01-24
 * 
 * @copyright Copyright (c) 2026
 * 
 */
#ifndef LIBCA_EM_MENU_MENU_H
#define LIBCA_EM_MENU_MENU_H

#include "../em_base/datatype.h"

// page不能大于255个
typedef u8 page_t;
// item不能大于65535个
typedef u16 item_t;

// 菜单的绘制上下文
typedef struct menu_context{
    // 绘制上下文相关接口
    // x和y是起始的坐标
    void (*draw_string)(u16 x, u16 y, const char* str);
    // 填充矩形
    void (*fill_rect)(u16 x, u16 y, u16 w, u16 h, u16 color);
    // render负责把缓冲区数据刷新到屏幕
    void (*render)(void);
    // 帧率控制，这个函数将会在刷新屏幕以后调用，结束以后才是下个循环
    void (*frame_control)(void);
}menu_context_t;

// 菜单结构体
typedef struct menu{
    // 屏幕的宽和高，以实际渲染
    u16 width;
    u16 height;
    u16 color_fg;       // 前景色 (字体颜色)
    u16 color_bg;       // 背景色
    u8  font_size;      // 字体大小 (12, 16, 24...)
    // 当前的页
    u8 current_page;
    // 当前光标位置，也就是选中的item
    u8 cursor_pos;
    // 是否应该退出menu的菜单循环
    u8 should_exit;
}menu_t;

// 菜单事件
typedef u16 menu_event_t;

#define MENU_EVENT_NONE 0
#define MENU_EVENT_UP 1
#define MENU_EVENT_DOWN 2
#define MENU_EVENT_BACK 3
#define MENU_EVENT_ENTER 4
// 64号开始的事件由user定义
#define MENU_EVENT_USER 64

void menu_exit(void);
void menu_loop(menu_context_t* ctx, menu_t* menu)
{
    while(!menu->should_exit){
        // 根据实际硬件情况调用menu_input_event输入事件
        // menu_input_event();

        // 拿到页面的渲染路由
        // menu_router_get(menu->current_page);

        ctx->frame_control();
    }
}



#endif
