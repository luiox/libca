#include "lcd1602.h"
#include "../em_base/debug.h"

static const lcd1602_port_t* g_port = NULL;

void lcd1602_bind_port(const lcd1602_port_t* port) {
    param_check(port != NULL);
    g_port = port;
}

bool lcd1602_port_is_registered(void) {
    return g_port != NULL;
}

/**
 * @brief 电平使能触发
 */
static void lcd1602_strobe(lcd1602_t* self) {
    param_check(self != NULL);
    g_port->write_pin(self->e_port, self->e_pin, 1);
    g_port->delay_us(2); // 使能脉冲宽度至少 450ns
    g_port->write_pin(self->e_port, self->e_pin, 0);
    g_port->delay_us(2);
}

/**
 * @brief 发送底层的 4位 或 8位 数据
 */
static void lcd1602_out(lcd1602_t* self, u8 data) {
    param_check(self != NULL);
    if (self->mode == LCD1602_MODE_8BIT) {
        for (u8 i = 0; i < 8; i++) {
            g_port->write_pin(self->data_ports[i], self->data_pins[i], (data >> i) & 0x01);
        }
    } else {
        // 4线模式下只使用了 data_ports[4-7] 对应的物理引脚，但在 self->data_pins[0-3] 中存储
        // 或者是使用者直接传了4个引脚。规范起见，假设4线模式使用 data_pins[0-3]
        for (u8 i = 0; i < 4; i++) {
            g_port->write_pin(self->data_ports[i], self->data_pins[i], (data >> i) & 0x01);
        }
    }
    lcd1602_strobe(self);
}

void lcd1602_write_cmd(lcd1602_t* self, u8 cmd) {
    param_check(self != NULL);
    if (!lcd1602_port_is_registered()) return;

    g_port->write_pin(self->rs_port, self->rs_pin, 0); // 命令模式
    if (self->mode == LCD1602_MODE_8BIT) {
        lcd1602_out(self, cmd);
    } else {
        lcd1602_out(self, (cmd >> 4) & 0x0F); // 先发高4位
        lcd1602_out(self, cmd & 0x0F);        // 再发低4位
    }
    g_port->delay_us(50); // 通用指令延时
}

void lcd1602_write_data(lcd1602_t* self, u8 data) {
    param_check(self != NULL);
    if (!lcd1602_port_is_registered()) return;

    g_port->write_pin(self->rs_port, self->rs_pin, 1); // 数据模式
    if (self->mode == LCD1602_MODE_8BIT) {
        lcd1602_out(self, data);
    } else {
        lcd1602_out(self, (data >> 4) & 0x0F);
        lcd1602_out(self, data & 0x0F);
    }
    g_port->delay_us(50);
}

void lcd1602_init(lcd1602_t* self) {
    param_check(self != NULL);
    if (!lcd1602_port_is_registered()) {
        debug_print("[lcd1602] error: port not registered\n");
        return;
    }

    // 设置 GPIO 为输出模式
    g_port->set_output_mode(self->rs_port, self->rs_pin);
    g_port->set_output_mode(self->e_port, self->e_pin);
    u8 count = (self->mode == LCD1602_MODE_8BIT) ? 8 : 4;
    for (u8 i = 0; i < count; i++) {
        g_port->set_output_mode(self->data_ports[i], self->data_pins[i]);
    }

    g_port->delay_ms(40); // 等待 VDD 稳定

    if (self->mode == LCD1602_MODE_8BIT) {
        lcd1602_write_cmd(self, 0x38); // 8-bit mode, 2 lines, 5x8 font
    } else {
        // 4线模式初始化流程
        g_port->write_pin(self->rs_port, self->rs_pin, 0);
        lcd1602_out(self, 0x03); 
        g_port->delay_ms(5);
        lcd1602_out(self, 0x03);
        g_port->delay_us(150);
        lcd1602_out(self, 0x03);
        lcd1602_out(self, 0x02); // 切换到4线模式

        lcd1602_write_cmd(self, 0x28); // 4-bit mode, 2 lines, 5x8 font
    }

    lcd1602_write_cmd(self, 0x0C); // 显示开，关光标，关闪烁
    lcd1602_write_cmd(self, 0x06); // 入口模式：光标右移，不滚屏
    lcd1602_clear(self);
}

void lcd1602_clear(lcd1602_t* self) {
    param_check(self != NULL);
    lcd1602_write_cmd(self, 0x01);
    g_port->delay_ms(2);
}

void lcd1602_set_cursor(lcd1602_t* self, u8 x, u8 y) {
    param_check(self != NULL);
	// 为了性能，而且这个坐标是常量的，所以这个坐标也在debug阶段通过param_check可以保证其合法性
	param_check(x < 16 && y < 2);

    u8 addr = (y == 0) ? (0x80 + x) : (0xC0 + x);
    lcd1602_write_cmd(self, addr);
}

void lcd1602_print_char(lcd1602_t* self, char ch) {
    param_check(self != NULL);
    lcd1602_write_data(self, (u8)ch);
}

void lcd1602_print(lcd1602_t* self, const char* str) {
    param_check(self != NULL);
    if (str == NULL) {
		debug_print("[lcd1602] in lcd1602_print, your str pointer is NULL\n");
        return;
    }
    while (*str) {
        lcd1602_write_data(self, (u8)*str++);
    }
}
