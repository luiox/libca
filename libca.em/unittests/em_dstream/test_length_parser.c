/* Auto-migrated from src/em_dstream/length_parser.c test blocks */
#include "length_parser.h"
#include <em_base/debug.h>

#include <em_test/test.h>

/* 使用 delimiter_parser.c 中定义的 mem_stream 实现 */
typedef struct
{
    u8*   buffer;
    usize capacity;
    usize used;
    usize cursor;
} mem_stream_t;

static usize mem_stream_capacity(dstream_t* self)
{
    mem_stream_t* ms = (mem_stream_t*)self->buf_obj;
    return ms->capacity;
}

static usize mem_stream_used(dstream_t* self)
{
    mem_stream_t* ms = (mem_stream_t*)self->buf_obj;
    return ms->used - ms->cursor;
}

static void mem_stream_skip(dstream_t* self, usize len)
{
    mem_stream_t* ms = (mem_stream_t*)self->buf_obj;
    if (ms->cursor + len > ms->used) {
        ms->cursor = ms->used;
    }
    else {
        ms->cursor += len;
    }
}

static void mem_stream_rewind(dstream_t* self, usize len)
{
    mem_stream_t* ms = (mem_stream_t*)self->buf_obj;
    if (ms->cursor < len) {
        ms->cursor = 0;
    }
    else {
        ms->cursor -= len;
    }
}

static usize mem_stream_offset(dstream_t* self)
{
    return 0;
}

static bool mem_stream_reset(dstream_t* self, usize pos)
{
    mem_stream_t* ms = (mem_stream_t*)self->buf_obj;
    if (pos > ms->used - ms->cursor) {
        return false;
    }
    ms->cursor += pos;
    return true;
}

static i32 mem_stream_read(dstream_t* self, void* dest, usize len)
{
    mem_stream_t* ms        = (mem_stream_t*)self->buf_obj;
    usize         available = ms->used - ms->cursor;
    usize         actual    = (len < available) ? len : available;
    memcpy(dest, ms->buffer + ms->cursor, actual);
    ms->cursor += actual;
    return (i32)actual;
}

static i32 mem_stream_peek(dstream_t* self, usize offset, void* dest, usize len)
{
    mem_stream_t* ms        = (mem_stream_t*)self->buf_obj;
    usize         available = ms->used - ms->cursor;
    if (offset >= available) {
        return 0;
    }
    usize actual = (len < available - offset) ? len : (available - offset);
    memcpy(dest, ms->buffer + ms->cursor + offset, actual);
    return (i32)actual;
}

static i32 mem_stream_write(dstream_t* self, const void* src, usize len)
{
    mem_stream_t* ms        = (mem_stream_t*)self->buf_obj;
    usize         available = ms->capacity - ms->used;
    usize         actual    = (len < available) ? len : available;
    memcpy(ms->buffer + ms->used, src, actual);
    ms->used += actual;
    return (i32)actual;
}

static dstream_ops_t mem_stream_ops = {
    .capacity = mem_stream_capacity,
    .used     = mem_stream_used,
    .skip     = mem_stream_skip,
    .rewind   = mem_stream_rewind,
    .offset   = mem_stream_offset,
    .reset    = mem_stream_reset,
    .read     = mem_stream_read,
    .peek     = mem_stream_peek,
    .write    = mem_stream_write,
};

static void mem_stream_init(mem_stream_t* ms, dstream_t* ds, u8* buffer, usize capacity)
{
    ms->buffer   = buffer;
    ms->capacity = capacity;
    ms->used     = 0;
    ms->cursor   = 0;
    ds->buf_obj  = ms;
    ds->ops      = &mem_stream_ops;
}

static void mem_stream_write_data(mem_stream_t* ms, const u8* data, usize len)
{
    usize available = ms->capacity - ms->used;
    usize actual    = (len < available) ? len : available;
    memcpy(ms->buffer + ms->used, data, actual);
    ms->used += actual;
}

/* 简单的校验和函数 */
static u8 simple_checksum_u8(const u8* data, usize len, u8 prev)
{
    u8 sum = prev;
    for (usize i = 0; i < len; i++) {
        sum += data[i];
    }
    return sum;
}

/* 测试用例 */

TEST_CASE(length_parser_no_checksum)
{
    u8           buffer[64];
    mem_stream_t ms;
    dstream_t    ds;
    mem_stream_init(&ms, &ds, buffer, sizeof(buffer));

    length_parser_t            parser;
    length_parser_cksum_func_t cksum_func = {.null_fn = NULL};
    length_parser_init(
        &parser, &ds, NULL, 0, 2, false, 0, false, LENGTH_PARSER_CKSUM_NONE, cksum_func, 0, 256);

    /* 帧: [len=4][data=0x01,0x02,0x03,0x04] */
    u8 frame[] = {0x04, 0x00, 0x01, 0x02, 0x03, 0x04};
    mem_stream_write_data(&ms, frame, sizeof(frame));

    usize len;
    TEST_ASSERT_EQUAL_INT(LENGTH_PARSER_OK, length_parser_get_frame(&parser, &len));
    TEST_ASSERT_EQUAL_UINT(4, len);

    length_parser_consume(&parser);
    TEST_ASSERT_EQUAL_UINT(0, dstream_used(&ds));
}

TEST_CASE(length_parser_with_header)
{
    u8           buffer[64];
    mem_stream_t ms;
    dstream_t    ds;
    mem_stream_init(&ms, &ds, buffer, sizeof(buffer));

    length_parser_t            parser;
    u8                         header[]   = {0x55, 0xAA};
    length_parser_cksum_func_t cksum_func = {.null_fn = NULL};
    length_parser_init(
        &parser, &ds, header, 2, 1, false, 0, false, LENGTH_PARSER_CKSUM_NONE, cksum_func, 0, 256);

    /* 帧: [header][len=3][data] */
    u8 frame[] = {0x55, 0xAA, 0x03, 0x01, 0x02, 0x03};
    mem_stream_write_data(&ms, frame, sizeof(frame));

    usize len;
    TEST_ASSERT_EQUAL_INT(LENGTH_PARSER_OK, length_parser_get_frame(&parser, &len));
    TEST_ASSERT_EQUAL_UINT(3, len);

    length_parser_consume(&parser);
    TEST_ASSERT_EQUAL_UINT(0, dstream_used(&ds));
}

TEST_CASE(length_parser_with_checksum)
{
    u8           buffer[64];
    mem_stream_t ms;
    dstream_t    ds;
    mem_stream_init(&ms, &ds, buffer, sizeof(buffer));

    length_parser_t            parser;
    length_parser_cksum_func_t cksum_func = {.checksum_u8 = simple_checksum_u8};
    length_parser_init(
        &parser, &ds, NULL, 0, 2, false, 1, false, LENGTH_PARSER_CKSUM_U8, cksum_func, 0, 256);

    /* 帧: [len=4][data][checksum] */
    /* data = 0x01,0x02,0x03,0x04, checksum = 0x0A */
    u8 frame[] = {0x04, 0x00, 0x01, 0x02, 0x03, 0x04, 0x0A};
    mem_stream_write_data(&ms, frame, sizeof(frame));

    usize len;
    TEST_ASSERT_EQUAL_INT(LENGTH_PARSER_OK, length_parser_get_frame(&parser, &len));
    TEST_ASSERT_EQUAL_UINT(4, len);

    length_parser_consume(&parser);
}

TEST_CASE(length_parser_checksum_error)
{
    u8           buffer[64];
    mem_stream_t ms;
    dstream_t    ds;
    mem_stream_init(&ms, &ds, buffer, sizeof(buffer));

    length_parser_t            parser;
    length_parser_cksum_func_t cksum_func = {.checksum_u8 = simple_checksum_u8};
    length_parser_init(
        &parser, &ds, NULL, 0, 2, false, 1, false, LENGTH_PARSER_CKSUM_U8, cksum_func, 0, 256);

    /* 帧: 错误的校验和 */
    u8 frame[] = {0x04, 0x00, 0x01, 0x02, 0x03, 0x04, 0xFF};
    mem_stream_write_data(&ms, frame, sizeof(frame));

    usize                  len;
    length_parser_result_t result = length_parser_get_frame(&parser, &len);
    TEST_ASSERT_EQUAL_INT(LENGTH_PARSER_ERR_CHECKSUM, result);
}

TEST_CASE(length_parser_invalid_len)
{
    u8           buffer[64];
    mem_stream_t ms;
    dstream_t    ds;
    mem_stream_init(&ms, &ds, buffer, sizeof(buffer));

    length_parser_t            parser;
    length_parser_cksum_func_t cksum_func = {.null_fn = NULL};
    length_parser_init(&parser,
                       &ds,
                       NULL,
                       0,
                       2,
                       false,
                       0,
                       false,
                       LENGTH_PARSER_CKSUM_NONE,
                       cksum_func,
                       0,
                       16); /* max=16 */

    /* 帧: len=100 超出 max_frame_len */
    u8 frame[] = {0x64, 0x00}; /* len = 100 */
    mem_stream_write_data(&ms, frame, sizeof(frame));

    usize                  len;
    length_parser_result_t result = length_parser_get_frame(&parser, &len);
    TEST_ASSERT_EQUAL_INT(LENGTH_PARSER_ERR_INVALID_LEN, result);
}

TEST_CASE(length_parser_header_mismatch)
{
    u8           buffer[64];
    mem_stream_t ms;
    dstream_t    ds;
    mem_stream_init(&ms, &ds, buffer, sizeof(buffer));

    length_parser_t            parser;
    u8                         header[]   = {0x55, 0xAA};
    length_parser_cksum_func_t cksum_func = {.null_fn = NULL};
    length_parser_init(
        &parser, &ds, header, 2, 1, false, 0, false, LENGTH_PARSER_CKSUM_NONE, cksum_func, 0, 256);

    /* 错误的帧头 */
    u8 frame[] = {0x55, 0xBB, 0x03, 0x01, 0x02, 0x03};
    mem_stream_write_data(&ms, frame, sizeof(frame));

    usize                  len;
    length_parser_result_t result = length_parser_get_frame(&parser, &len);
    TEST_ASSERT_EQUAL_INT(LENGTH_PARSER_ERR_SYNC, result);
}

TEST_CASE(length_parser_partial_data)
{
    u8           buffer[64];
    mem_stream_t ms;
    dstream_t    ds;
    mem_stream_init(&ms, &ds, buffer, sizeof(buffer));

    length_parser_t            parser;
    length_parser_cksum_func_t cksum_func = {.null_fn = NULL};
    length_parser_init(
        &parser, &ds, NULL, 0, 2, false, 0, false, LENGTH_PARSER_CKSUM_NONE, cksum_func, 0, 256);

    usize len;

    /* 分多次写入数据 */
    mem_stream_write_data(&ms, (u8*)"\x04\x00", 2); /* len 字段 */
    TEST_ASSERT_EQUAL_INT(LENGTH_PARSER_NEED_MORE, length_parser_get_frame(&parser, &len));

    mem_stream_write_data(&ms, (u8*)"\x01\x02", 2); /* 部分数据 */
    TEST_ASSERT_EQUAL_INT(LENGTH_PARSER_NEED_MORE, length_parser_get_frame(&parser, &len));

    mem_stream_write_data(&ms, (u8*)"\x03\x04", 2); /* 剩余数据 */
    TEST_ASSERT_EQUAL_INT(LENGTH_PARSER_OK, length_parser_get_frame(&parser, &len));
    TEST_ASSERT_EQUAL_UINT(4, len);

    length_parser_consume(&parser);
}

TEST_CASE(length_parser_big_endian)
{
    u8           buffer[64];
    mem_stream_t ms;
    dstream_t    ds;
    mem_stream_init(&ms, &ds, buffer, sizeof(buffer));

    length_parser_t            parser;
    length_parser_cksum_func_t cksum_func = {.null_fn = NULL};
    length_parser_init(&parser,
                       &ds,
                       NULL,
                       0,
                       2,
                       true,
                       0,
                       false, /* len_big_endian=true */
                       LENGTH_PARSER_CKSUM_NONE,
                       cksum_func,
                       0,
                       256);

    /* 大端序: len = 0x0004 */
    u8 frame[] = {0x00, 0x04, 0x01, 0x02, 0x03, 0x04};
    mem_stream_write_data(&ms, frame, sizeof(frame));

    usize len;
    TEST_ASSERT_EQUAL_INT(LENGTH_PARSER_OK, length_parser_get_frame(&parser, &len));
    TEST_ASSERT_EQUAL_UINT(4, len);

    length_parser_consume(&parser);
}

TEST_CASE(length_parser_multiple_frames)
{
    u8           buffer[64];
    mem_stream_t ms;
    dstream_t    ds;
    mem_stream_init(&ms, &ds, buffer, sizeof(buffer));

    length_parser_t            parser;
    length_parser_cksum_func_t cksum_func = {.null_fn = NULL};
    length_parser_init(
        &parser, &ds, NULL, 0, 1, false, 0, false, LENGTH_PARSER_CKSUM_NONE, cksum_func, 0, 256);

    /* 两个帧 */
    u8 frames[] = {0x02, 0xAB, 0xCD, 0x03, 0x01, 0x02, 0x03};
    mem_stream_write_data(&ms, frames, sizeof(frames));

    usize len;

    /* 第一帧 */
    TEST_ASSERT_EQUAL_INT(LENGTH_PARSER_OK, length_parser_get_frame(&parser, &len));
    TEST_ASSERT_EQUAL_UINT(2, len);
    length_parser_consume(&parser);

    /* 第二帧 */
    TEST_ASSERT_EQUAL_INT(LENGTH_PARSER_OK, length_parser_get_frame(&parser, &len));
    TEST_ASSERT_EQUAL_UINT(3, len);
    length_parser_consume(&parser);

    TEST_ASSERT_EQUAL_UINT(0, dstream_used(&ds));
}
