#ifndef MYLIB_UTILITY_HEAP_H
#define MYLIB_UTILITY_HEAP_H

#include "../em_base/datatype.h"

void heap_init(void *buf, uint32_t size);

void *heap_alloc(uint32_t size);

void heap_free(void *ptr);


#endif // !MYLIB_UTILITY_HEAP_H

