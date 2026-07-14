#include "length_parser.h"
#include <em_base/debug.h>

///
/// @brief 从流中读取长度字段（支持部分读取）
/// @param self 解析器对象
/// @param available 当前可用数据长度
/// @return true 继续读取，false 需要更多数据或完成
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

///
/// @brief 计算数据的校验值
/// @param self 解析器对象
/// @param data 数据指针
/// @param len 数据长度
/// @param prev 上一次计算的校验值
/// @return 新的校验值
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

///
/// @brief 从流中读取校验值
/// @param self 解析器对象
/// @param offset 校验字段在流中的偏移
/// @return 校验值
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

///
/// @brief 错误恢复：跳过一字节并重置状态
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

