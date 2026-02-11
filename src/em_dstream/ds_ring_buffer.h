/**
 * @file ds_ring_buffer.h
 * @brief dstream adapter for ring_buffer
 */
#ifndef LIBCA_EM_DSTREAM_DS_RING_BUFFER_H
#define LIBCA_EM_DSTREAM_DS_RING_BUFFER_H

#include "dstream.h"

const dstream_ops_t* ring_buf_get_dstream_ops(void);

#endif // !LIBCA_EM_DSTREAM_DS_RING_BUFFER_H