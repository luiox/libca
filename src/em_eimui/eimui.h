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

// 菜单事件
typedef u16 menu_event_t;

#define MENU_EVENT_NONE 0
#define MENU_EVENT_UP 1
#define MENU_EVENT_DOWN 2
#define MENU_EVENT_BACK 3
#define MENU_EVENT_ENTER 4
// 64号开始的事件由user定义
#define MENU_EVENT_USER 64

// 菜单的绘制上下文
typedef struct menu_context{
    // 绘制上下文相关接口
    // x和y是起始的坐标
    void (*draw_string)(u16 x, u16 y, const char* str, u16 color_fg, u16 color_bg);
    // 填充矩形
    void (*fill_rect)(u16 x, u16 y, u16 w, u16 h, u16 color);
    // render负责把缓冲区数据刷新到屏幕
    void (*render)(void);
    // 帧率控制，这个函数将会在刷新屏幕以后调用，结束以后才是下个循环
    void (*frame_control)(void);
}menu_context_t;

struct menu;

// 页面处理函数，负责当前页面的逻辑和绘制
typedef void (*menu_page_handler_t)(menu_context_t* ctx, struct menu* menu);

// 菜单结构体
typedef struct menu{
    // 屏幕的宽和高，以实际渲染
    u16 width;
    u16 height;
    u16 color_fg;       // 前景色 (字体颜色)
    u16 color_bg;       // 背景色
    u8  font_size;      // 字体大小 (12, 16, 24...)
    
    // 当前的页 ID
    page_t current_page;
    // 上一个页面 ID (用于返回)
    page_t last_page;
    
    // 当前光标位置，也就是选中的item索引
    item_t cursor_pos;
    // 在当前页面的起始渲染偏移 (用于翻页)
    item_t scroll_offset;

    // 是否应该退出menu的菜单循环
    u8 should_exit;
    // 是否应该重新绘制
    u8 should_repaint;
    
    // 最近一次发生的事件
    menu_event_t event;

    // 用户私有数据，可以用于传递给自定义页面
    void* user_data;
}menu_t;

/**
 * @brief 初始化菜单
 */
void menu_init(menu_t* menu, u16 w, u16 h);

/**
 * @brief 执行一次菜单逻辑 (Tick)
 */
void menu_tick(menu_context_t* ctx, menu_t* menu);

/**
 * @brief 设置当前页面
 */
void menu_set_page(menu_t* menu, page_t page_id);

/**
 * @brief 退出菜单
 */
void menu_exit(menu_t* menu);

/**
 * @brief 输入事件
 */
void menu_input_event(menu_t* menu, menu_event_t event);


// 外部实现
void menu_route_handler(menu_context_t* ctx, struct menu* menu);


#endif
