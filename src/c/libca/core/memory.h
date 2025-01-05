#ifndef MEMORY_H
#define MEMORY_H

#include <libca/stdtype.h>

typedef struct
{
    uint8_t* ptr;   // 指向用于分配的内存块

} memory_pool_t;

#endif
