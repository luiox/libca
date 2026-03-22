/* Auto-migrated from src/em_dstream/delimiter_parser.c test blocks */
#include "delimiter_parser.h"
#include <em_base/debug.h>

#include <em_test/test.h>

/* 测试用的内存流实现 */
typedef struct {
    u8* buffer;
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
    } else {
        ms->cursor += len;
    }
}

static void mem_stream_rewind(dstream_t* self, usize len)
{
    mem_stream_t* ms = (mem_stream_t*)self->buf_obj;
    if (ms->cursor < len) {
        ms->cursor = 0;
    } else {
        ms->cursor -= len;
    }
}

static usize mem_stream_offset(dstream_t* self)
{
    mem_stream_t* ms = (mem_stream_t*)self->buf_obj;
    return 0; // 简化：offset 相对于当前 cursor
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
    mem_stream_t* ms = (mem_stream_t*)self->buf_obj;
    usize available = ms->used - ms->cursor;
    usize actual = (len < available) ? len : available;
    memcpy(dest, ms->buffer + ms->cursor, actual);
    ms->cursor += actual;
    return (i32)actual;
}

static i32 mem_stream_peek(dstream_t* self, usize offset, void* dest, usize len)
{
    mem_stream_t* ms = (mem_stream_t*)self->buf_obj;
    usize available = ms->used - ms->cursor;
    if (offset >= available) {
        return 0;
    }
    usize actual = (len < available - offset) ? len : (available - offset);
    memcpy(dest, ms->buffer + ms->cursor + offset, actual);
    return (i32)actual;
}

static i32 mem_stream_write(dstream_t* self, const void* src, usize len)
{
    mem_stream_t* ms = (mem_stream_t*)self->buf_obj;
    usize available = ms->capacity - ms->used;
    usize actual = (len < available) ? len : available;
    memcpy(ms->buffer + ms->used, src, actual);
    ms->used += actual;
    return (i32)actual;
}

static dstream_ops_t mem_stream_ops = {
    .capacity = mem_stream_capacity,
    .used = mem_stream_used,
    .skip = mem_stream_skip,
    .rewind = mem_stream_rewind,
    .offset = mem_stream_offset,
    .reset = mem_stream_reset,
    .read = mem_stream_read,
    .peek = mem_stream_peek,
    .write = mem_stream_write,
};

static void mem_stream_init(mem_stream_t* ms, dstream_t* ds, u8* buffer, usize capacity)
{
    ms->buffer = buffer;
    ms->capacity = capacity;
    ms->used = 0;
    ms->cursor = 0;
    ds->buf_obj = ms;
    ds->ops = &mem_stream_ops;
}

static void mem_stream_write_data(mem_stream_t* ms, const u8* data, usize len)
{
    usize available = ms->capacity - ms->used;
    usize actual = (len < available) ? len : available;
    memcpy(ms->buffer + ms->used, data, actual);
    ms->used += actual;
}

/* 测试用例 */

TEST_CASE(delimiter_parser_trailer_only)
{
    u8 buffer[64];
    mem_stream_t ms;
    dstream_t ds;
    mem_stream_init(&ms, &ds, buffer, sizeof(buffer));

    delimiter_parser_t parser;
    u8 trailer[] = "\r\n";
    delimiter_parser_init(&parser, &ds, NULL, 0, trailer, 2, 32);

    // 写入数据：AT指令
    mem_stream_write_data(&ms, (u8*)"AT+RST\r\n", 8);

    usize len;
    TEST_ASSERT_EQUAL_INT(DELIMITER_PARSER_OK, delimiter_parser_get_frame(&parser, &len));
    TEST_ASSERT_EQUAL_UINT(8, len);

    delimiter_parser_consume(&parser);

    // 验证 cursor 已移动
    TEST_ASSERT_EQUAL_UINT(0, dstream_used(&ds));
}

TEST_CASE(delimiter_parser_header_only)
{
    // 注意：只有头部没有尾部时，帧无法自动结束
    // 解析器会持续等待更多数据，直到上层决定处理
    // 这种场景需要配合超时或上层协议处理
    u8 buffer[64];
    mem_stream_t ms;
    dstream_t ds;
    mem_stream_init(&ms, &ds, buffer, sizeof(buffer));

    delimiter_parser_t parser;
    u8 header[] = {0x55, 0xAA};
    delimiter_parser_init(&parser, &ds, header, 2, NULL, 0, 32);

    // 写入数据：0x55 0xAA + 4字节数据
    mem_stream_write_data(&ms, (u8*)"\x55\xAA\x01\x02\x03\x04", 6);

    usize len;
    // 由于没有尾部，解析器无法确定帧结束，持续返回 NEED_MORE
    TEST_ASSERT_EQUAL_INT(DELIMITER_PARSER_NEED_MORE, delimiter_parser_get_frame(&parser, &len));
}

TEST_CASE(delimiter_parser_header_and_trailer)
{
    u8 buffer[64];
    mem_stream_t ms;
    dstream_t ds;
    mem_stream_init(&ms, &ds, buffer, sizeof(buffer));

    delimiter_parser_t parser;
    u8 header[] = {0x7E};
    u8 trailer[] = {0x7E};
    delimiter_parser_init(&parser, &ds, header, 1, trailer, 1, 32);

    // 写入数据：0x7E + 数据 + 0x7E + 额外字节（触发尾部匹配完成）
    // 注意：当头部和尾部相同时，需要在尾部后添加数据来触发帧检测
    mem_stream_write_data(&ms, (u8*)"\x7E\x01\x02\x03\x7E\xFF", 6);

    usize len;
    TEST_ASSERT_EQUAL_INT(DELIMITER_PARSER_OK, delimiter_parser_get_frame(&parser, &len));
    TEST_ASSERT_EQUAL_UINT(5, len); // \x7E\x01\x02\x03\x7E

    delimiter_parser_consume(&parser);

    // 验证剩余数据
    TEST_ASSERT_EQUAL_UINT(1, dstream_used(&ds));
}

TEST_CASE(delimiter_parser_partial_data)
{
    u8 buffer[64];
    mem_stream_t ms;
    dstream_t ds;
    mem_stream_init(&ms, &ds, buffer, sizeof(buffer));

    delimiter_parser_t parser;
    u8 trailer[] = "\r\n";
    delimiter_parser_init(&parser, &ds, NULL, 0, trailer, 2, 32);

    // 分多次写入数据
    mem_stream_write_data(&ms, (u8*)"AT", 2);

    usize len;
    TEST_ASSERT_EQUAL_INT(DELIMITER_PARSER_NEED_MORE, delimiter_parser_get_frame(&parser, &len));

    mem_stream_write_data(&ms, (u8*)"+RST", 4);
    TEST_ASSERT_EQUAL_INT(DELIMITER_PARSER_NEED_MORE, delimiter_parser_get_frame(&parser, &len));

    mem_stream_write_data(&ms, (u8*)"\r\n", 2);
    TEST_ASSERT_EQUAL_INT(DELIMITER_PARSER_OK, delimiter_parser_get_frame(&parser, &len));
    TEST_ASSERT_EQUAL_UINT(8, len);

    delimiter_parser_consume(&parser);
}

TEST_CASE(delimiter_parser_frame_too_long)
{
    u8 buffer[64];
    mem_stream_t ms;
    dstream_t ds;
    mem_stream_init(&ms, &ds, buffer, sizeof(buffer));

    delimiter_parser_t parser;
    u8 trailer[] = "\r\n";
    delimiter_parser_init(&parser, &ds, NULL, 0, trailer, 2, 8); // max_frame_len = 8

    // 写入超过最大长度的数据（无尾部）
    mem_stream_write_data(&ms, (u8*)"1234567890ABCDEF", 16);

    usize len;
    delimiter_parser_result_t result = delimiter_parser_get_frame(&parser, &len);
    // 应该返回超长错误
    TEST_ASSERT_EQUAL_INT(DELIMITER_PARSER_ERR_FRAME_TOO_LONG, result);
}

TEST_CASE(delimiter_parser_trailer_backtrack)
{
    // 测试尾部匹配回溯：\r\r\n 应该正确匹配
    u8 buffer[64];
    mem_stream_t ms;
    dstream_t ds;
    mem_stream_init(&ms, &ds, buffer, sizeof(buffer));

    delimiter_parser_t parser;
    u8 trailer[] = "\r\n";
    delimiter_parser_init(&parser, &ds, NULL, 0, trailer, 2, 32);

    // 写入数据：包含 \r\r\n
    mem_stream_write_data(&ms, (u8*)"DATA\r\r\n", 7);

    usize len;
    TEST_ASSERT_EQUAL_INT(DELIMITER_PARSER_OK, delimiter_parser_get_frame(&parser, &len));
    TEST_ASSERT_EQUAL_UINT(7, len); // "DATA\r\r\n" 总长度

    delimiter_parser_consume(&parser);
}

TEST_CASE(delimiter_parser_multiple_frames)
{
    u8 buffer[64];
    mem_stream_t ms;
    dstream_t ds;
    mem_stream_init(&ms, &ds, buffer, sizeof(buffer));

    delimiter_parser_t parser;
    u8 trailer[] = "\r\n";
    delimiter_parser_init(&parser, &ds, NULL, 0, trailer, 2, 32);

    // 写入多个帧
    mem_stream_write_data(&ms, (u8*)"AT\r\nOK\r\n", 8);

    usize len;
    u8 frame_buf[16];

    // 第一帧
    TEST_ASSERT_EQUAL_INT(DELIMITER_PARSER_OK, delimiter_parser_get_frame(&parser, &len));
    TEST_ASSERT_EQUAL_UINT(4, len);
    dstream_peek(&ds, 0, frame_buf, len);
    TEST_ASSERT_EQUAL_MEMORY("AT\r\n", frame_buf, 4);
    delimiter_parser_consume(&parser);

    // 第二帧
    TEST_ASSERT_EQUAL_INT(DELIMITER_PARSER_OK, delimiter_parser_get_frame(&parser, &len));
    TEST_ASSERT_EQUAL_UINT(4, len);
    dstream_peek(&ds, 0, frame_buf, len);
    TEST_ASSERT_EQUAL_MEMORY("OK\r\n", frame_buf, 4);
    delimiter_parser_consume(&parser);
}

TEST_CASE(delimiter_parser_header_skip_garbage)
{
    u8 buffer[64];
    mem_stream_t ms;
    dstream_t ds;
    mem_stream_init(&ms, &ds, buffer, sizeof(buffer));

    delimiter_parser_t parser;
    u8 header[] = {0x55, 0xAA};
    u8 trailer[] = {0x0D, 0x0A};
    delimiter_parser_init(&parser, &ds, header, 2, trailer, 2, 32);

    // 写入垃圾数据 + 有效帧
    mem_stream_write_data(&ms, (u8*)"\xFF\xFF\x55\xAA\x01\x02\x0D\x0A", 8);

    usize len;
    TEST_ASSERT_EQUAL_INT(DELIMITER_PARSER_OK, delimiter_parser_get_frame(&parser, &len));
    TEST_ASSERT_EQUAL_UINT(6, len); // \x55\xAA\x01\x02\x0D\x0A

    delimiter_parser_consume(&parser);
}

