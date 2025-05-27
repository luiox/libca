#ifndef FIXED_SIZE_BUFFER_H
#define FIXED_SIZE_BUFFER_H

#include <cstdint>
#include <stdint.h>

typedef struct {
    uint8_t* buffer;
    uint32_t capacity;
    uint32_t head;
    uint32_t tail;
}fixed_size_buffer_t;




#endif // !FIXED_SIZE_BUFFER_H
