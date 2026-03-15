#include "length_parser.h"
#include <em_base/debug.h>

/**
 * @brief 从流中读取长度字段（支持部分读取）
 * @param self 解析器对象
 * @param available 当前可用数据长度
 * @return true 继续读取，false 需要更多数据或完成
 */
static bool read_len_field(length_parser_t* self, usize available)
{
    while (self->partial_count < self->len_field_size) {
        usize peek_offset = self->header_len + self->partial_count;
        if (peek_offset >= available) {
            return false; // 数据不足
        }

        u8 byte = dstream_peek_u8(self->ds, peek_offset);
        if (self->len_big_endian) {
            self->len_accumulator = (self->len_accumulator << 8) | byte;
        } else {
            self->len_accumulator |= ((u32)byte << (self->partial_count * 8));
        }
        self->partial_count++;
    }
    return true;
}

/**
 * @brief 计算数据的校验值
 * @param self 解析器对象
 * @param data 数据指针
 * @param len 数据长度
 * @param prev 上一次计算的校验值
 * @return 新的校验值
 */
static u32 calc_checksum(length_parser_t* self, const void* data, usize len, u32 prev)
{
    switch (self->cksum_type) {
    case LENGTH_PARSER_CKSUM_U8:
        return self->cksum_func.checksum_u8((const u8*)data, len, (u8)prev);
    case LENGTH_PARSER_CKSUM_CRC16:
        return self->cksum_func.crc16(data, len, (u16)prev);
    case LENGTH_PARSER_CKSUM_CRC32:
        return self->cksum_func.crc32(data, len, prev);
    default:
        return 0;
    }
}

/**
 * @brief 从流中读取校验值
 * @param self 解析器对象
 * @param offset 校验字段在流中的偏移
 * @return 校验值
 */
static u32 read_checksum_from_stream(length_parser_t* self, usize offset)
{
    u32 checksum = 0;
    for (u8 i = 0; i < self->checksum_size; i++) {
        u8 byte = dstream_peek_u8(self->ds, offset + i);
        if (self->checksum_big_endian) {
            checksum = (checksum << 8) | byte;
        } else {
            checksum |= ((u32)byte << (i * 8));
        }
    }
    return checksum;
}

/**
 * @brief 错误恢复：跳过一字节并重置状态
 */
static void recover_from_error(length_parser_t* self)
{
    dstream_skip(self->ds, 1);
    length_parser_reset(self);
}

void length_parser_init(length_parser_t* self, dstream_t* ds,
                        const u8* header, usize header_len,
                        u8 len_field_size, bool len_big_endian,
                        u8 checksum_size, bool checksum_big_endian,
                        length_parser_cksum_type_t cksum_type,
                        length_parser_cksum_func_t cksum_func,
                        u32 cksum_init_val,
                        usize max_frame_len)
{
    param_check(self != NULL);
    param_check(ds != NULL);
    param_check(len_field_size == 1 || len_field_size == 2 || len_field_size == 4);
    param_check(checksum_size == 0 || checksum_size == 1 || 
                checksum_size == 2 || checksum_size == 4);

    self->ds = ds;
    self->header = header;
    self->header_len = header_len;
    self->len_field_size = len_field_size;
    self->len_big_endian = len_big_endian;
    self->checksum_size = checksum_size;
    self->checksum_big_endian = checksum_big_endian;
    self->cksum_type = cksum_type;
    self->cksum_func = cksum_func;
    self->cksum_init_val = cksum_init_val;
    self->max_frame_len = max_frame_len;

    length_parser_reset(self);
}

length_parser_result_t length_parser_get_frame(length_parser_t* self, usize* out_len)
{
    param_check(self != NULL);
    param_check(self->ds != NULL);
    param_check(out_len != NULL);

    dstream_t* ds = self->ds;

    while (true) {
        usize available = dstream_used(ds);

        switch (self->state) {
        case LENGTH_STATE_IDLE: {
            /* 检查帧头 */
            if (self->header != NULL && self->header_len > 0) {
                if (available < self->header_len) {
                    return LENGTH_PARSER_NEED_MORE;
                }

                /* 匹配帧头 */
                bool match = true;
                for (usize i = 0; i < self->header_len; i++) {
                    if (dstream_peek_u8(ds, i) != self->header[i]) {
                        match = false;
                        break;
                    }
                }

                if (!match) {
                    recover_from_error(self);
                    return LENGTH_PARSER_ERR_SYNC;
                }
            }

            /* 检查是否有足够数据读取长度字段 */
            usize len_field_offset = self->header_len;
            if (available < len_field_offset + self->len_field_size) {
                return LENGTH_PARSER_NEED_MORE;
            }

            /* 读取长度字段 */
            self->len_accumulator = 0;
            self->partial_count = 0;
            if (!read_len_field(self, available)) {
                self->state = LENGTH_STATE_LEN_PARTIAL;
                return LENGTH_PARSER_NEED_MORE;
            }

            /* 验证长度 */
            if (self->len_accumulator > self->max_frame_len) {
                recover_from_error(self);
                return LENGTH_PARSER_ERR_INVALID_LEN;
            }

            self->target_len = self->len_accumulator;
            self->calc_checksum = self->cksum_init_val;
            self->state = LENGTH_STATE_DATA_AND_CKSUM;
            break;
        }

        case LENGTH_STATE_LEN_PARTIAL: {
            /* 继续读取长度字段 */
            if (!read_len_field(self, available)) {
                return LENGTH_PARSER_NEED_MORE;
            }

            /* 验证长度 */
            if (self->len_accumulator > self->max_frame_len) {
                recover_from_error(self);
                return LENGTH_PARSER_ERR_INVALID_LEN;
            }

            self->target_len = self->len_accumulator;
            self->calc_checksum = self->cksum_init_val;
            self->state = LENGTH_STATE_DATA_AND_CKSUM;
            break;
        }

        case LENGTH_STATE_DATA_AND_CKSUM: {
            usize data_offset = self->header_len + self->len_field_size;
            usize total_needed = data_offset + self->target_len + self->checksum_size;

            if (available < total_needed) {
                return LENGTH_PARSER_NEED_MORE;
            }

            /* 计算校验值（如果有校验） */
            if (self->checksum_size > 0 && self->cksum_type != LENGTH_PARSER_CKSUM_NONE) {
                /* 只对数据部分计算校验（不包含长度字段） */
                for (u32 i = 0; i < self->target_len; i++) {
                    u8 byte = dstream_peek_u8(ds, data_offset + i);
                    self->calc_checksum = calc_checksum(self, &byte, 1, self->calc_checksum);
                }

                /* 读取期望的校验值 */
                usize checksum_offset = data_offset + self->target_len;
                self->expected_checksum = read_checksum_from_stream(self, checksum_offset);

                /* 根据校验类型截取有效位进行比较 */
                u32 calc_val = self->calc_checksum;
                u32 expect_val = self->expected_checksum;
                bool checksum_ok = false;

                switch (self->cksum_type) {
                case LENGTH_PARSER_CKSUM_U8:
                    checksum_ok = ((u8)calc_val == (u8)expect_val);
                    break;
                case LENGTH_PARSER_CKSUM_CRC16:
                    checksum_ok = ((u16)calc_val == (u16)expect_val);
                    break;
                case LENGTH_PARSER_CKSUM_CRC32:
                    checksum_ok = (calc_val == expect_val);
                    break;
                default:
                    checksum_ok = true;
                    break;
                }

                if (!checksum_ok) {
                    recover_from_error(self);
                    return LENGTH_PARSER_ERR_CHECKSUM;
                }
            }

            /* 帧就绪 */
            *out_len = self->target_len;
            return LENGTH_PARSER_OK;
        }

        default:
            length_parser_reset(self);
            return LENGTH_PARSER_ERR_SYNC;
        }
    }
}

void length_parser_consume(length_parser_t* self)
{
    param_check(self != NULL);
    param_check(self->ds != NULL);

    usize total_len = self->header_len + self->len_field_size + 
                      self->target_len + self->checksum_size;
    dstream_skip(self->ds, total_len);

    length_parser_reset(self);
}

void length_parser_reset(length_parser_t* self)
{
    param_check(self != NULL);

    self->state = LENGTH_STATE_IDLE;
    self->partial_count = 0;
    self->len_accumulator = 0;
    self->target_len = 0;
    self->calc_checksum = 0;
    self->expected_checksum = 0;
}

#if TEST_ENABLE
#include <em_test/test.h>

/* 使用 delimiter_parser.c 中定义的 mem_stream 实现 */
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
    u8 buffer[64];
    mem_stream_t ms;
    dstream_t ds;
    mem_stream_init(&ms, &ds, buffer, sizeof(buffer));

    length_parser_t parser;
    length_parser_cksum_func_t cksum_func = { .null_fn = NULL };
    length_parser_init(&parser, &ds, NULL, 0, 2, false, 0, false,
                       LENGTH_PARSER_CKSUM_NONE, cksum_func, 0, 256);

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
    u8 buffer[64];
    mem_stream_t ms;
    dstream_t ds;
    mem_stream_init(&ms, &ds, buffer, sizeof(buffer));

    length_parser_t parser;
    u8 header[] = {0x55, 0xAA};
    length_parser_cksum_func_t cksum_func = { .null_fn = NULL };
    length_parser_init(&parser, &ds, header, 2, 1, false, 0, false,
                       LENGTH_PARSER_CKSUM_NONE, cksum_func, 0, 256);

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
    u8 buffer[64];
    mem_stream_t ms;
    dstream_t ds;
    mem_stream_init(&ms, &ds, buffer, sizeof(buffer));

    length_parser_t parser;
    length_parser_cksum_func_t cksum_func = { .checksum_u8 = simple_checksum_u8 };
    length_parser_init(&parser, &ds, NULL, 0, 2, false, 1, false,
                       LENGTH_PARSER_CKSUM_U8, cksum_func, 0, 256);

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
    u8 buffer[64];
    mem_stream_t ms;
    dstream_t ds;
    mem_stream_init(&ms, &ds, buffer, sizeof(buffer));

    length_parser_t parser;
    length_parser_cksum_func_t cksum_func = { .checksum_u8 = simple_checksum_u8 };
    length_parser_init(&parser, &ds, NULL, 0, 2, false, 1, false,
                       LENGTH_PARSER_CKSUM_U8, cksum_func, 0, 256);

    /* 帧: 错误的校验和 */
    u8 frame[] = {0x04, 0x00, 0x01, 0x02, 0x03, 0x04, 0xFF};
    mem_stream_write_data(&ms, frame, sizeof(frame));

    usize len;
    length_parser_result_t result = length_parser_get_frame(&parser, &len);
    TEST_ASSERT_EQUAL_INT(LENGTH_PARSER_ERR_CHECKSUM, result);
}

TEST_CASE(length_parser_invalid_len)
{
    u8 buffer[64];
    mem_stream_t ms;
    dstream_t ds;
    mem_stream_init(&ms, &ds, buffer, sizeof(buffer));

    length_parser_t parser;
    length_parser_cksum_func_t cksum_func = { .null_fn = NULL };
    length_parser_init(&parser, &ds, NULL, 0, 2, false, 0, false,
                       LENGTH_PARSER_CKSUM_NONE, cksum_func, 0, 16); /* max=16 */

    /* 帧: len=100 超出 max_frame_len */
    u8 frame[] = {0x64, 0x00}; /* len = 100 */
    mem_stream_write_data(&ms, frame, sizeof(frame));

    usize len;
    length_parser_result_t result = length_parser_get_frame(&parser, &len);
    TEST_ASSERT_EQUAL_INT(LENGTH_PARSER_ERR_INVALID_LEN, result);
}

TEST_CASE(length_parser_header_mismatch)
{
    u8 buffer[64];
    mem_stream_t ms;
    dstream_t ds;
    mem_stream_init(&ms, &ds, buffer, sizeof(buffer));

    length_parser_t parser;
    u8 header[] = {0x55, 0xAA};
    length_parser_cksum_func_t cksum_func = { .null_fn = NULL };
    length_parser_init(&parser, &ds, header, 2, 1, false, 0, false,
                       LENGTH_PARSER_CKSUM_NONE, cksum_func, 0, 256);

    /* 错误的帧头 */
    u8 frame[] = {0x55, 0xBB, 0x03, 0x01, 0x02, 0x03};
    mem_stream_write_data(&ms, frame, sizeof(frame));

    usize len;
    length_parser_result_t result = length_parser_get_frame(&parser, &len);
    TEST_ASSERT_EQUAL_INT(LENGTH_PARSER_ERR_SYNC, result);
}

TEST_CASE(length_parser_partial_data)
{
    u8 buffer[64];
    mem_stream_t ms;
    dstream_t ds;
    mem_stream_init(&ms, &ds, buffer, sizeof(buffer));

    length_parser_t parser;
    length_parser_cksum_func_t cksum_func = { .null_fn = NULL };
    length_parser_init(&parser, &ds, NULL, 0, 2, false, 0, false,
                       LENGTH_PARSER_CKSUM_NONE, cksum_func, 0, 256);

    usize len;

    /* 分多次写入数据 */
    mem_stream_write_data(&ms, (u8*)"\x04\x00", 2);  /* len 字段 */
    TEST_ASSERT_EQUAL_INT(LENGTH_PARSER_NEED_MORE, length_parser_get_frame(&parser, &len));

    mem_stream_write_data(&ms, (u8*)"\x01\x02", 2);  /* 部分数据 */
    TEST_ASSERT_EQUAL_INT(LENGTH_PARSER_NEED_MORE, length_parser_get_frame(&parser, &len));

    mem_stream_write_data(&ms, (u8*)"\x03\x04", 2);  /* 剩余数据 */
    TEST_ASSERT_EQUAL_INT(LENGTH_PARSER_OK, length_parser_get_frame(&parser, &len));
    TEST_ASSERT_EQUAL_UINT(4, len);

    length_parser_consume(&parser);
}

TEST_CASE(length_parser_big_endian)
{
    u8 buffer[64];
    mem_stream_t ms;
    dstream_t ds;
    mem_stream_init(&ms, &ds, buffer, sizeof(buffer));

    length_parser_t parser;
    length_parser_cksum_func_t cksum_func = { .null_fn = NULL };
    length_parser_init(&parser, &ds, NULL, 0, 2, true, 0, false,  /* len_big_endian=true */
                       LENGTH_PARSER_CKSUM_NONE, cksum_func, 0, 256);

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
    u8 buffer[64];
    mem_stream_t ms;
    dstream_t ds;
    mem_stream_init(&ms, &ds, buffer, sizeof(buffer));

    length_parser_t parser;
    length_parser_cksum_func_t cksum_func = { .null_fn = NULL };
    length_parser_init(&parser, &ds, NULL, 0, 1, false, 0, false,
                       LENGTH_PARSER_CKSUM_NONE, cksum_func, 0, 256);

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

#endif // TEST_ENABLE
