/**
 * @file eimui.h
 * @author canrad (1517807724@qq.com)
 * @brief 数据驱动的MCU下的菜单系统 (eimui)
 * 保证对RAM的使用尽可能小，无动画，支持子菜单、翻页，选项行为
 * @version 0.1
 * @date 2026-01-24
 * 
 * @copyright Copyright (c) 2026
 * 
 */
#ifndef LIBCA_EM_EIMUI_H
#define LIBCA_EM_EIMUI_H

#include <em_base/datatype.h>

// page不能大于255个
typedef u8 page_t;
// item不能大于65535个
typedef u16 item_t;

// 菜单事件
typedef u16 eimui_event_t;

#define EIMUI_EVENT_NONE 0
#define EIMUI_EVENT_UP 1
#define EIMUI_EVENT_DOWN 2
#define EIMUI_EVENT_BACK 3
#define EIMUI_EVENT_ENTER 4
// 64号开始的事件由user定义
#define EIMUI_EVENT_USER 64

// 菜单的绘制上下文
typedef struct eimui_context{
    // 绘制操作集，由user定义
    void* dops;
    // render负责把缓冲区数据刷新到屏幕
    void (*render)(void);
    // 帧率控制，这个函数将会在刷新屏幕以后调用，结束以后才是下个循环
    void (*frame_control)(void);
}eimui_context_t;

// 菜单结构体
typedef struct eimui{
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
    bool should_exit;
    // 是否应该重新绘制
    bool should_repaint;
    
    // 最近一次发生的事件
    eimui_event_t event;

    // 用户私有数据，可以用于传递给自定义页面
    void* user_data;
}eimui_t;

/**
 * @brief 初始化UI
 */
void eimui_init(eimui_t* ui, u16 w, u16 h);

/**
 * @brief 执行一次UI逻辑 (Tick)
 */
void eimui_tick(eimui_context_t* ctx, eimui_t* self);

/**
 * @brief 设置当前页面
 */
void eimui_set_page(eimui_t* self, page_t page_id);

/**
 * @brief 退出UI
 */
void eimui_exit(eimui_t* self);

/**
 * @brief 重绘
 */
void eimui_repaint(eimui_t* self);

/**
 * @brief 输入事件
 */
void eimui_input_event(eimui_t* self, eimui_event_t event);

/**
 * @brief handler分发器，由router.c实现
 * 
 * @param dops 
 * @param self 
 */
void eimui_route_handler(void* dops, eimui_t* self);


#endif // LIBCA_EM_EIMUI_H
