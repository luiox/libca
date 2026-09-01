/// @file ssd1306.h
/// @author Canrad
/// @brief ssd1306 OLED显示驱动
/// @version 0.1
/// @date 2026-03-18
///
/// @copyright Copyright (c) 2026
///
#ifndef LIBCA_EM_DRIVER_SSD1306_H
#define LIBCA_EM_DRIVER_SSD1306_H

#include <em_base/datatype.h>

typedef struct ssd1306_port
{
    void (*delay_ms)(u32 ms);
    void (*i2c_mem_write)(void* hi2c, u16 dev_addr, u8 mem_addr, u16 mem_addr_size, u8* data,
                          u16 data_size, u32 timeout);
} ssd1306_port_t;

void ssd1306_bind_port(const ssd1306_port_t* port);
bool ssd1306_port_is_registered(void);

#define OLED_FONT_6X8 12
#define OLED_FONT_8X16 16

/// @function: void ssd1306_init(void* i2c_handle)
/// @description: OLED初始化
/// @param {void*} i2c_handle I2C handle/pointer used by the driver
void ssd1306_init(void* i2c_handle);

/// @function: void ssd1306_clear(void)
/// @description: 清屏,整个屏幕是黑色的!和没点亮一样!!!
void ssd1306_clear(void);

/// @function: void ssd1306_display_on(void)
/// @description: 开启OLED显示
void ssd1306_display_on(void);

/// @function: void ssd1306_display_off(void)
/// @description: 关闭OLED显示
void ssd1306_display_off(void);

/// @function: void ssd1306_set_pos(u8 x, u8 y)
/// @description: 坐标设置
/// @param {u8} x 列坐标
/// @param {u8} y 页坐标（0~7）
void ssd1306_set_pos(u8 x, u8 y);

/// @function: void ssd1306_update(void)
/// @description: 更新显示（将缓存写入屏幕）
void ssd1306_update(void);

/// @function: void ssd1306_show_num(u8 x, u8 y, u32 num, u8 len, u8 font_size, u8 color_turn)
/// @description: 显示数字
/// @param {u8} x 起始横坐标
/// @param {u8} y 起始纵坐标
/// @param {u32} num 要显示的数字
/// @param {u8} len 位数
/// @param {u8} font_size 字体大小（16或12）
/// @param {u8} color_turn 是否反相显示（1反相、0不反相）
void ssd1306_show_num(u8 x, u8 y, u32 num, u8 len, u8 font_size, u8 color_turn);

/// @function: void ssd1306_show_decimal(u8 x, u8 y, float num, u8 z_len, u8 f_len, u8 font_size, u8
/// color_turn)
/// @description: 显示正负浮点数
/// @param {u8} x 起始横坐标
/// @param {u8} y 起始纵坐标
/// @param {float} num 浮点数
/// @param {u8} z_len 整数部分位数
/// @param {u8} f_len 小数部分位数
/// @param {u8} font_size 字体大小
/// @param {u8} color_turn 是否反相显示（1反相、0不反相）
void ssd1306_show_decimal(u8 x, u8 y, float num, u8 z_len, u8 f_len, u8 font_size, u8 color_turn);

/// @function: void ssd1306_show_char(u8 x, u8 y, u8 chr, u8 char_size, u8 color_turn)
/// @description: 在OLED指定位置显示一个字符
/// @param {u8} x 横坐标
/// @param {u8} y 纵坐标
/// @param {u8} chr 字符
/// @param {u8} char_size 字体大小（16或12）
/// @param {u8} color_turn 是否反相显示（1反相、0不反相）
void ssd1306_show_char(u8 x, u8 y, u8 chr, u8 char_size, u8 color_turn);

/// @function: void ssd1306_show_string(u8 x, u8 y, char* str, u8 char_size, u8 color_turn)
/// @description: 在OLED指定位置显示字符串
/// @param {u8} x 起始横坐标
/// @param {u8} y 起始纵坐标
/// @param {char*} str 字符串（以 '\0' 结尾）
/// @param {u8} char_size 字体大小
/// @param {u8} color_turn 是否反相显示（1反相、0不反相）
void ssd1306_show_string(u8 x, u8 y, char* str, u8 char_size, u8 color_turn);

/// @function: void ssd1306_draw_bmp(u8 x0, u8 y0, u8 x1, u8 y1, u8* bmp, u8 color_turn)
/// @description: 在OLED特定区域显示BMP图片
/// @param {u8} x0 起始横坐标
/// @param {u8} y0 起始纵坐标（页）
/// @param {u8} x1 结束横坐标
/// @param {u8} y1 结束纵坐标（页）
/// @param {u8*} bmp 图像数据
/// @param {u8} color_turn 是否反相显示（1反相、0不反相）
void ssd1306_draw_bmp(u8 x0, u8 y0, u8 x1, u8 y1, u8* bmp, u8 color_turn);

/// @function: void ssd1306_horizontal_shift(u8 dir)
/// @description: 全屏水平滚动显示
/// @param {u8} dir 滚动方向（LEFT 0x27，RIGHT 0x26）
void ssd1306_horizontal_shift(u8 dir);

/// @function: void ssd1306_some_horizontal_shift(u8 dir, u8 start_page, u8 end_page)
/// @description: 部分区域水平滚动显示
/// @param {u8} dir 滚动方向
/// @param {u8} start_page 开始页地址（0x00~0x07）
/// @param {u8} end_page 结束页地址（0x01~0x07）
void ssd1306_some_horizontal_shift(u8 dir, u8 start_page, u8 end_page);

/// @function: void ssd1306_vertical_and_horizontal_shift(u8 dir)
/// @description: 垂直并水平组合滚动显示
/// @param {u8} dir 滚动模式（例如右上0x29，左上0x2A）
void ssd1306_vertical_and_horizontal_shift(u8 dir);

/// @function: void ssd1306_display_mode(u8 mode)
/// @description: 屏幕显示模式（反色/正常）
/// @param {u8} mode ON=0xA7（反相），OFF=0xA6（正常）
void ssd1306_display_mode(u8 mode);

/// @function: void ssd1306_intensity_control(u8 intensity)
/// @description: 调整屏幕亮度
/// @param {u8} intensity 亮度值（0x00~0xFF）
void ssd1306_intensity_control(u8 intensity);


#endif   // !LIBCA_EM_DRIVER_SSD1306_H
