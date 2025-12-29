#ifndef FIXED_SIZE_BUFFER_H
#define FIXED_SIZE_BUFFER_H

#include <stdint.h>

typedef struct fixed_size_buffer {
    uint8_t* buffer;
    uint32_t capacity;
    uint32_t head;
    uint32_t tail;
}fixed_size_buffer_t;




#endif // !FIXED_SIZE_BUFFER_H
