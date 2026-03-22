#include "delimiter_parser.h"
#include <em_base/debug.h>

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

        switch (self->state) {
        case DELIM_STATE_IDLE: {
            if (self->header != NULL && self->header_len > 0) {
                /* 统一的头部匹配逻辑：逐字节匹配，支持部分匹配 */
                bool matched_this_round = true;
                
                for (usize i = self->match_len; i < self->header_len; i++) {
                    if (i >= available) {
                        /* 数据不足，保存当前匹配进度，等待更多数据 */
                        matched_this_round = false;
                        return DELIMITER_PARSER_NEED_MORE;
                    }
                    
                    u8 byte = dstream_peek_u8(ds, i);
                    if (byte == self->header[self->match_len]) {
                        self->match_len++;
                    } else {
                        /* 匹配失败，跳过已检查的第一个字节，重置状态 */
                        dstream_skip(ds, 1);
                        self->match_len = 0;
                        matched_this_round = false;
                        break;  /* 继续外层 while，重新尝试匹配 */
                    }
                }
                
                if (matched_this_round && self->match_len == self->header_len) {
                    /* 头部完整匹配 */
                    self->match_len = 0;
                    self->current_frame_len = self->header_len;
                    self->state = DELIM_STATE_IN_FRAME;
                }
            } else {
                /* 无头部，直接进入帧内 */
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

