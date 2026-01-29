#include "fixed_size_buffer.h"

void fsb_init(fixed_size_buffer_t* self, u8* data, usize capacity)
{
    self->raw = data;
    self->capacity = capacity;
    self->len = 0;
    self->cursor = 0;
}
