#ifndef PRINTER_H
#define PRINTER_H

#include "datatype.h"
#include <stdarg.h>

typedef struct {
    u8* buf;
    usize buf_size;
    void (*output_func)(u8* buf, usize size);
}printer_t;

void printer_init(printer_t* printer, u8* buf, usize buf_size, void (*output_func)(u8* buf, usize size));

void printer_println(printer_t* printer, const char* str);

void printer_printf(printer_t* printer, const char* fmt, ...);

void printer_vprintf(printer_t* printer, const char* fmt, va_list args);

#endif // !PRINTER_H