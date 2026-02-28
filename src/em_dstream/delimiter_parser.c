#include "delimiter_parser.h"
#include "../em_base/debug.h"

/**
 * @brief 检查字节是否匹配定界符的指定位置
 * @param delim 定界符字节数组
 * @param delim_len 定界符长度
 * @param byte 待检查字节
 * @param pos 定界符中的位置
 * @return 是否匹配
 */
static bool match_delim_byte(const u8* delim, usize delim_len, u8 byte, usize pos)
{
    if (delim == NULL || delim_len == 0 || pos >= delim_len) {
        return false;
    }
    return delim[pos] == byte;
}

/* 无效匹配位置标记 */
#define DELIM_NO_MATCH ((usize)-1)

/**
 * @brief 计算字节可能作为定界符新的起始匹配位置
 * @param delim 定界符
 * @param delim_len 定界符长度
 * @param byte 待检查字节
 * @return 新的匹配位置（0表示可以重新开始匹配，DELIM_NO_MATCH表示无法匹配）
 * @note 处理部分匹配失败后的回溯，例如匹配 "\r\n" 时遇到 "\r\r"，
 *       第一个 \r 失败后，第二个 \r 可以作为新的起始
 */
static usize find_new_match_pos(const u8* delim, usize delim_len, u8 byte)
{
    if (delim == NULL || delim_len == 0) {
        return DELIM_NO_MATCH;
    }
    // 检查该字节是否可以匹配定界符的开头
    if (delim[0] == byte) {
        return 0;
    }
    return DELIM_NO_MATCH;
}

void delimiter_parser_init(delimiter_parser_t* self, dstream_t* ds,
                           const u8* header, usize header_len,
                           const u8* trailer, usize trailer_len,
                           usize max_frame_len)
{
    param_check(self != NULL);
    param_check(ds != NULL);
    // 至少需要一个定界符
    param_check(header != NULL || trailer != NULL);

    self->ds = ds;
    self->header = header;
    self->header_len = header_len;
    self->trailer = trailer;
    self->trailer_len = trailer_len;
    self->max_frame_len = max_frame_len;

    self->state = DELIM_STATE_IDLE;
    self->match_len = 0;
    self->current_frame_len = 0;
}

delimiter_parser_result_t delimiter_parser_get_frame(delimiter_parser_t* self, usize* out_len)
{
    param_check(self != NULL);
    param_check(self->ds != NULL);
    param_check(out_len != NULL);

    dstream_t* ds = self->ds;

    // 如果已有完整帧等待消费，直接返回
    if (self->state == DELIM_STATE_FRAME_READY) {
        *out_len = self->current_frame_len;
        return DELIMITER_PARSER_OK;
    }

    while (true) {
        // 获取当前可用的数据长度
        usize available = dstream_used(ds);
        usize cursor_offset = dstream_offset(ds);

        switch (self->state) {
        case DELIM_STATE_IDLE: {
            // 需要匹配头部
            if (self->header != NULL && self->header_len > 0) {
                // 检查是否有足够数据匹配头部
                if (available < self->header_len) {
                    // 尝试部分匹配
                    usize match_start = self->match_len;
                    for (usize i = match_start; i < available; i++) {
                        u8 byte = dstream_peek_u8(ds, i);
                        if (byte == self->header[self->match_len]) {
                            self->match_len++;
                            if (self->match_len == self->header_len) {
                                // 完整匹配头部
                                self->match_len = 0;
                                self->current_frame_len = self->header_len;
                                self->state = DELIM_STATE_IN_FRAME;
                                break; // 跳出 for 循环，继续外层 while
                            }
                        } else {
                            // 头部匹配失败，跳过已检查的字节
                            usize skip_len = (self->match_len > 0) ? 1 : (i + 1);
                            dstream_skip(ds, skip_len);
                            self->match_len = 0;
                            break; // 跳出 for 循环，继续外层 while
                        }
                    }
                    // 如果 for 循环正常结束（未找到完整头部）且数据不足
                    if (self->state == DELIM_STATE_IDLE) {
                        return DELIMITER_PARSER_NEED_MORE;
                    }
                } else {
                    // 数据足够，尝试完整匹配头部
                    bool match = true;
                    for (usize i = 0; i < self->header_len; i++) {
                        if (dstream_peek_u8(ds, i) != self->header[i]) {
                            match = false;
                            break;
                        }
                    }
                    if (match) {
                        self->current_frame_len = self->header_len;
                        self->state = DELIM_STATE_IN_FRAME;
                    } else {
                        // 匹配失败，跳过一个字节继续寻找
                        dstream_skip(ds, 1);
                    }
                }
            } else {
                // 无头部，直接进入帧内
                self->current_frame_len = 0;
                self->state = DELIM_STATE_IN_FRAME;
            }
            break;
        }

        case DELIM_STATE_IN_FRAME: {
            // 有尾部需要匹配
            if (self->trailer != NULL && self->trailer_len > 0) {
                usize peek_offset = self->current_frame_len;

                // 检查是否还有数据可读
                if (peek_offset >= available) {
                    return DELIMITER_PARSER_NEED_MORE;
                }

                // 超长检测
                if (self->current_frame_len >= self->max_frame_len) {
                    // 跳过帧起始的一个字节（滑动窗口），重置状态机
                    dstream_skip(ds, 1);
                    delimiter_parser_reset(self);
                    return DELIMITER_PARSER_ERR_FRAME_TOO_LONG;
                }

                u8 byte = dstream_peek_u8(ds, peek_offset);
                self->current_frame_len++;

                if (byte == self->trailer[0]) {
                    // 开始匹配尾部
                    self->match_len = 1;
                    // 如果尾部只有一个字节，匹配完成
                    if (self->match_len == self->trailer_len) {
                        *out_len = self->current_frame_len;
                        self->state = DELIM_STATE_FRAME_READY;
                        return DELIMITER_PARSER_OK;
                    }
                    self->state = DELIM_STATE_TRAILER_MATCH;
                }
                // 否则继续在帧内
            } else {
                // 无尾部时，帧无法自动结束，持续等待更多数据
                // 上层需要通过超时或其他机制决定帧结束
                return DELIMITER_PARSER_NEED_MORE;
            }
            break;
        }

        case DELIM_STATE_TRAILER_MATCH: {
            usize peek_offset = self->current_frame_len;

            // 检查是否还有数据可读
            if (peek_offset >= available) {
                return DELIMITER_PARSER_NEED_MORE;
            }

            // 超长检测
            if (self->current_frame_len >= self->max_frame_len) {
                dstream_skip(ds, 1);
                delimiter_parser_reset(self);
                return DELIMITER_PARSER_ERR_FRAME_TOO_LONG;
            }

            u8 byte = dstream_peek_u8(ds, peek_offset);
            self->current_frame_len++;

            if (byte == self->trailer[self->match_len]) {
                self->match_len++;
                if (self->match_len == self->trailer_len) {
                    // 完整匹配尾部，帧就绪
                    *out_len = self->current_frame_len;
                    self->state = DELIM_STATE_FRAME_READY;
                    return DELIMITER_PARSER_OK;
                }
                // 继续匹配尾部下一个字节
            } else {
                // 尾部匹配失败，检查当前字节是否可以作为新的尾部起始
                usize new_pos = find_new_match_pos(self->trailer, self->trailer_len, byte);
                if (new_pos != DELIM_NO_MATCH) {
                    self->match_len = new_pos + 1; // 已经匹配了第一个字节
                    // 状态保持 TRAILER_MATCH
                } else {
                    // 当前字节不匹配尾部开头，回到 IN_FRAME 状态
                    self->match_len = 0;
                    self->state = DELIM_STATE_IN_FRAME;
                }
            }
            break;
        }

        case DELIM_STATE_FRAME_READY:
            // 不会到达这里，已在函数开头处理
            *out_len = self->current_frame_len;
            return DELIMITER_PARSER_OK;

        default:
            return DELIMITER_PARSER_ERR_INTERNAL;
        }
    }
}

void delimiter_parser_consume(delimiter_parser_t* self)
{
    param_check(self != NULL);
    param_check(self->ds != NULL);

    if (self->state == DELIM_STATE_FRAME_READY && self->current_frame_len > 0) {
        dstream_skip(self->ds, self->current_frame_len);
    }

    // 重置状态机
    self->state = DELIM_STATE_IDLE;
    self->match_len = 0;
    self->current_frame_len = 0;
}

void delimiter_parser_reset(delimiter_parser_t* self)
{
    param_check(self != NULL);

    self->state = DELIM_STATE_IDLE;
    self->match_len = 0;
    self->current_frame_len = 0;
}

#if TEST_ENABLE
#include "../em_test/test.h"

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

#endif // TEST_ENABLE
