#include "printer.h"
#include <stdio.h>
#include <string.h>
#include <stdarg.h>

void printer_init(printer_t* printer, u8* buf, usize buf_size,
                  void (*output_func)(u8* buf, usize size))
{
    printer->output_func = output_func;
    printer->buf         = buf;
    printer->buf_size    = buf_size;
}

void printer_println(printer_t* printer, const char* str) 
{
    printer->output_func((u8*)str, strlen(str)); 
    printer->output_func((u8*)"\n", 1);  
}

void printer_vprintf(printer_t* printer, const char* fmt, va_list args) 
{
    // 使用 vsprintf 将参数格式化到 buffer 中
    vsprintf((char*)printer->buf, fmt, args);

    // 输出格式化后的字符串
    printer->output_func(printer->buf, strlen((char*)printer->buf));
}

void printer_printf(printer_t* printer, const char* fmt, ...) 
{
    va_list args; // 定义一个 va_list 类型的变量，用来存放参数

    va_start(args, fmt); // 初始化 args，fmt 是最后一个固定参数

    printer_vprintf(printer, fmt, args);

    va_end(args); // 清理 args
}
