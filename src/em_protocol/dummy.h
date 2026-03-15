#ifndef LIBCA_EM_PROTOCOL_DUMMY_H
#define LIBCA_EM_PROTOCOL_DUMMY_H

#include "file_transfer.h"
#include "em_base/datatype.h"

typedef struct dummy {
    transport_t* io;
    file_transfer_cbs_t cbs;
    void* user_data;
} dummy_t;

// Initialize dummy internal data structures
void dummy_proto_init(dummy_t* d);

// file_transfer compatible ops
i32 dummy_init(void* self, transport_t* io, const file_transfer_cbs_t* cbs, void* user_data);
i32 dummy_process(void* self, const u8* in_buf, usize in_len);
i32 dummy_tick(void* self, u32 ms_delta);
void dummy_start_recv(void* self);
void dummy_start_send(void* self, const char* filename, u32 file_size);
i32 dummy_get_transferred_size(void* self);

extern const file_transfer_ops_t g_dummy_ops;

#endif // LIBCA_EM_PROTOCOL_DUMMY_H
