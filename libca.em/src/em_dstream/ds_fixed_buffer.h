///
/// @file ds_fixed_buffer.h
/// @author Canrad
/// @brief 为了fixed_buffer适配dstream的包装
/// @version 0.1
/// @date 2026-02-11
///
/// @copyright Copyright (c) 2026
///
#ifndef LIBCA_EM_DSTREAM_DS_FIXED_BUFFER_H
#define LIBCA_EM_DSTREAM_DS_FIXED_BUFFER_H

#include "dstream.h"

const dstream_ops_t* fixed_buf_get_dstream_ops(void);


#endif   // !LIBCA_EM_DSTREAM_DS_FIXED_BUFFER_H
