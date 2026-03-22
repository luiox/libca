#include "ssd1306.h"
#include "ssd1306_font.h"

#define OLED_I2C_ADDRESS 0x78
#define OLED_I2C_TIMEOUT 0x100

static ssd1306_port_t* g_ssd1306_port = NULL;

void ssd1306_bind_port(const ssd1306_port_t* port)
{
    g_ssd1306_port = (ssd1306_port_t*)port;
}

bool ssd1306_port_is_registered(void)
{
    return (g_ssd1306_port != NULL);
}

void* g_hi2c = NULL;

/**
 * @function: void ssd1306_write_command(u8 cmd)
 * @description: 向设备写控制命令
 * @param {u8} cmd 芯片手册规定的命令
 * @return {*}
 */
void ssd1306_write_command(u8 cmd)
{
    g_ssd1306_port->i2c_mem_write(g_hi2c, OLED_I2C_ADDRESS, 0x00, 1, &cmd, 1, OLED_I2C_TIMEOUT);
}

/**
 * @function: void ssd1306_write_data(u8 data)
 * @description: 向设备写控制数据
 * @param {u8} data 数据
 * @return {*}
 */
void ssd1306_write_data(u8 data)
{
    g_ssd1306_port->i2c_mem_write(g_hi2c, OLED_I2C_ADDRESS, 0x40, 1, &data, 1, OLED_I2C_TIMEOUT);
}

/**********************************************************
 * 初始化命令,根据芯片手册书写，详细步骤见上图以及注意事项
 ***********************************************************/
u8 g_init_cmds[] = {0xAE, 0xD5, 0x80, 0xA8, 0x3F, 0xD3, 0x00, 0x40, 0xA1, 0xC8, 0xDA, 0x12,
                         0x81, 0xCF, 0xD9, 0xF1, 0xDB, 0x40, 0xA4, 0xA6, 0x8D, 0x14, 0xAF};

void ssd1306_init(void* i2c_handle)
{
    g_hi2c = i2c_handle;
    g_ssd1306_port->delay_ms(200);

    u8 i = 0;
    for (i = 0; i < 23; i++) {
        ssd1306_write_command(g_init_cmds[i]);
    }
}

void ssd1306_update(void)
{
    u8 i, n;
    for (i = 0; i < 8; i++) {
        ssd1306_write_command(0xb0 + i);   // 设置页地址（0~7）
        ssd1306_write_command(0x00);       // 设置显示位置―列低地址
        ssd1306_write_command(0x10);       // 设置显示位置―列高地址
        for (n = 0; n < 128; n++)
            ssd1306_write_data(1);
    }
}


void ssd1306_clear(void)
{
    u8 i, n;
    for (i = 0; i < 8; i++) {
        ssd1306_write_command(0xb0 + i);   // 设置页地址（0~7）
        ssd1306_write_command(0x00);       // 设置显示位置―列低地址
        ssd1306_write_command(0x10);       // 设置显示位置―列高地址
        for (n = 0; n < 128; n++)
            ssd1306_write_data(0);
    }
}

void ssd1306_display_on(void)
{
    ssd1306_write_command(0X8D);   // SET DCDC命令
    ssd1306_write_command(0X14);   // DCDC ON
    ssd1306_write_command(0XAF);   // DISPLAY ON,打开显示
}


void ssd1306_display_off(void)
{
    ssd1306_write_command(0X8D);   // SET DCDC命令
    ssd1306_write_command(0X10);   // DCDC OFF
    ssd1306_write_command(0XAE);   // DISPLAY OFF，关闭显示
}

void ssd1306_set_pos(u8 x, u8 y)
{
    ssd1306_write_command(0xb0 + y);                   // 设置页地址（0~7）
    ssd1306_write_command(((x & 0xf0) >> 4) | 0x10);   // 设置显示位置―列高地址
    ssd1306_write_command(x & 0x0f);                   // 设置显示位置―列低地址
}


u32 ssd1306_pow(u8 m, u8 n)
{
    u32 result = 1;
    while (n--)
        result *= m;
    return result;
}

void ssd1306_show_char(u8 x, u8 y, u8 chr, u8 char_size, u8 color_turn)
{
    u8 c = 0, i = 0;
    c = chr - ' ';   // 得到偏移后的值
    if (x > 128 - 1) {
        x = 0;
        y = y + 2;
    }
    if (char_size == 16) {
        ssd1306_set_pos(x, y);
        for (i = 0; i < 8; i++) {
            if (color_turn)
                ssd1306_write_data(~F8X16[c * 16 + i]);
            else
                ssd1306_write_data(F8X16[c * 16 + i]);
        }
        ssd1306_set_pos(x, y + 1);
        for (i = 0; i < 8; i++) {
            if (color_turn)
                ssd1306_write_data(~F8X16[c * 16 + i + 8]);
            else
                ssd1306_write_data(F8X16[c * 16 + i + 8]);
        }
    }
    else {
        ssd1306_set_pos(x, y);
        for (i = 0; i < 6; i++) {
            if (color_turn)
                ssd1306_write_data(~F6x8[c][i]);
            else
                ssd1306_write_data(F6x8[c][i]);
        }
    }
}

void ssd1306_show_string(u8 x, u8 y, char* str, u8 char_size, u8 color_turn)
{
    u8 j = 0;
    while (str[j] != '\0') {
        ssd1306_show_char(x, y, str[j], char_size, color_turn);
        if (char_size == OLED_FONT_6X8)   // 6X8的字体列加6，显示下一个字符
            x += 6;
        else   // 8X16的字体列加8，显示下一个字符
            x += 8;

        if (x > 122 && char_size == 12)   // TextSize6x8如果一行不够显示了，从下一行继续显示
        {
            x = 0;
            y++;
        }
        if (x > 120 && char_size == 16)   // TextSize8x16如果一行不够显示了，从下一行继续显示
        {
            x = 0;
            y++;
        }
        j++;
    }
}

void ssd1306_show_num(u8 x, u8 y, u32 num, u8 len, u8 font_size, u8 color_turn)
{
    u8 t, temp;
    u8 enshow = 0;
    for (t = 0; t < len; t++) {
        temp = (num / ssd1306_pow(10, len - t - 1)) % 10;
        if (enshow == 0 && t < (len - 1)) {
            if (temp == 0) {
                ssd1306_show_char(x + (font_size / 2) * t, y, ' ', font_size, color_turn);
                continue;
            }
            else
                enshow = 1;
        }
        ssd1306_show_char(x + (font_size / 2) * t, y, temp + '0', font_size, color_turn);
    }
}


void ssd1306_show_decimal(u8 x, u8 y, float num, u8 z_len, u8 f_len, u8 font_size, u8 color_turn)
{
    u8 t, temp, i = 0;   // i为负数标志位
    u8 enshow;
    int z_temp, f_temp;
    if (num < 0) {
        z_len += 1;
        i = 1;
        num = -num;
    }
    z_temp = (int)num;
    // 整数部分
    for (t = 0; t < z_len; t++) {
        temp = (z_temp / ssd1306_pow(10, z_len - t - 1)) % 10;
        if (enshow == 0 && t < (z_len - 1)) {
            if (temp == 0) {
                ssd1306_show_char(x + (font_size / 2) * t, y, ' ', font_size, color_turn);
                continue;
            }
            else
                enshow = 1;
        }
        ssd1306_show_char(x + (font_size / 2) * t, y, temp + '0', font_size, color_turn);
    }
    // 小数点
    ssd1306_show_char(x + (font_size / 2) * (z_len), y, '.', font_size, color_turn);

    f_temp = (int)((num - z_temp) * (ssd1306_pow(10, f_len)));
    // 小数部分
    for (t = 0; t < f_len; t++) {
        temp = (f_temp / ssd1306_pow(10, f_len - t - 1)) % 10;
        ssd1306_show_char(x + (font_size / 2) * (t + z_len) + 5, y, temp + '0', font_size, color_turn);
    }
    if (i == 1)   // 如果为负，就将最前的一位赋值‘-’
    {
        ssd1306_show_char(x, y, '-', font_size, color_turn);
        i = 0;
    }
}

void ssd1306_draw_bmp(u8 x0, u8 y0, u8 x1, u8 y1, u8* bmp, u8 color_turn)
{
    u32 j = 0;
    u8 x = 0, y = 0;

    if (y1 % 8 == 0)
        y = y1 / 8;
    else
        y = y1 / 8 + 1;
    for (y = y0; y < y1; y++) {
        ssd1306_set_pos(x0, y);
        for (x = x0; x < x1; x++) {
            if (color_turn)
                ssd1306_write_data(~bmp[j++]);   // 显示反相图片
            else
                ssd1306_write_data(bmp[j++]);   // 显示图片
        }
    }
}



void ssd1306_horizontal_shift(u8 dir)
{
    ssd1306_write_command(0x2e);        // 停止滚动
    ssd1306_write_command(dir);   // 设置滚动方向
    ssd1306_write_command(0x00);        // 虚拟字节设置，默认为0x00
    ssd1306_write_command(0x00);        // 设置开始页地址
    ssd1306_write_command(0x07);        // 设置每个滚动步骤之间的时间间隔的帧频
    //  0x00-5帧， 0x01-64帧， 0x02-128帧， 0x03-256帧， 0x04-3帧， 0x05-4帧， 0x06-25帧， 0x07-2帧，
    ssd1306_write_command(0x07);   // 设置结束页地址
    ssd1306_write_command(0x00);   // 虚拟字节设置，默认为0x00
    ssd1306_write_command(0xff);   // 虚拟字节设置，默认为0xff
    ssd1306_write_command(0x2f);   // 开启滚动-0x2f，禁用滚动-0x2e，禁用需要重写数据
}


void ssd1306_some_horizontal_shift(u8 dir, u8 start_page, u8 end_page)
{
    ssd1306_write_command(0x2e);        // 停止滚动
    ssd1306_write_command(dir);   // 设置滚动方向
    ssd1306_write_command(0x00);        // 虚拟字节设置，默认为0x00
    ssd1306_write_command(start_page);       // 设置开始页地址
    ssd1306_write_command(0x07);        // 设置每个滚动步骤之间的时间间隔的帧频,0x07即滚动速度2帧
    ssd1306_write_command(end_page);         // 设置结束页地址
    ssd1306_write_command(0x00);        // 虚拟字节设置，默认为0x00
    ssd1306_write_command(0xff);        // 虚拟字节设置，默认为0xff
    ssd1306_write_command(0x2f);        // 开启滚动-0x2f，禁用滚动-0x2e，禁用需要重写数据
}


void ssd1306_vertical_and_horizontal_shift(u8 dir)
{
    ssd1306_write_command(0x2e);        // 停止滚动
    ssd1306_write_command(dir);   // 设置滚动方向
    ssd1306_write_command(0x01);        // 虚拟字节设置
    ssd1306_write_command(0x00);        // 设置开始页地址
    ssd1306_write_command(0x07);        // 设置每个滚动步骤之间的时间间隔的帧频，即滚动速度
    ssd1306_write_command(0x07);        // 设置结束页地址
    ssd1306_write_command(0x01);        // 垂直滚动偏移量
    ssd1306_write_command(0x00);        // 虚拟字节设置，默认为0x00
    ssd1306_write_command(0xff);        // 虚拟字节设置，默认为0xff
    ssd1306_write_command(0x2f);        // 开启滚动-0x2f，禁用滚动-0x2e，禁用需要重写数据
}

void ssd1306_display_mode(u8 mode)
{
    ssd1306_write_command(mode);
}

void ssd1306_intensity_control(u8 intensity)
{
    ssd1306_write_command(0x81);
    ssd1306_write_command(intensity);
}
