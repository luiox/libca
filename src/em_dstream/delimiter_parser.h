/**
 * @file delimiter_parser.h
 * @author canrad (1517807724@qq.com)
 * @brief 定界符解析器
 * @version 0.1
 * @date 2026-02-28
 * 
 * @copyright Copyright (c) 2026
 * 
 */
#ifndef LIBCA_EM_DSTREAM_DELIMITER_PARSER_H
#define LIBCA_EM_DSTREAM_DELIMITER_PARSER_H

#include "em_base/datatype.h"
#include "dstream.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 定界符解析器结果码
 */
typedef enum delimiter_parser_result_enum {
    DELIMITER_PARSER_OK = 0,                /**< 成功找到完整帧 */
    DELIMITER_PARSER_NEED_MORE,             /**< 数据不足，需等待更多数据 */
    DELIMITER_PARSER_ERR_FRAME_TOO_LONG,    /**< 帧长度超出 max_frame_len */
    DELIMITER_PARSER_ERR_INTERNAL,          /**< 内部错误（参数非法等） */
} delimiter_parser_result_t;

typedef struct delimiter_parser delimiter_parser_t;

/**
 * @brief 定界符解析器状态
 */
typedef enum delimiter_parser_state_enum {
    DELIM_STATE_IDLE,                /**< 空闲：寻找头部（若有）或直接进入帧 */
    DELIM_STATE_IN_FRAME,            /**< 已进入帧，正在寻找尾部 */
    DELIM_STATE_TRAILER_MATCH,       /**< 部分匹配尾部 */
    DELIM_STATE_FRAME_READY,         /**< 完整帧已找到 */
} delimiter_parser_state_t;

/**
 * @brief 定界符解析器
 */
struct delimiter_parser {
    dstream_t* ds;                  /**< 关联的数据流 */
    const u8* header;               /**< 头部定界符（可为NULL） */
    usize header_len;               /**< 头部长度 */
    const u8* trailer;              /**< 尾部定界符（可为NULL） */
    usize trailer_len;              /**< 尾部长度 */
    usize max_frame_len;            /**< 允许的最大帧总长度（含头部和尾部） */

    delimiter_parser_state_t state; /**< 状态机状态 */
    usize match_len;                /**< 当前已匹配的定界符字节数 */
    usize current_frame_len;        /**< 从帧起始开始已累积的字节数 */
};

/**
 * @brief 初始化定界符解析器
 * @param self        解析器对象
 * @param ds          数据流
 * @param header      头部定界符（可为NULL，此时必须提供尾部）
 * @param header_len  头部长度
 * @param trailer     尾部定界符（可为NULL，此时必须提供头部）
 * @param trailer_len 尾部长度
 * @param max_frame_len 最大允许帧长度（包含头部和尾部），超过此值视为错误
 * @note 必须至少提供一个定界符（头部或尾部），否则无法定界
 */
void delimiter_parser_init(delimiter_parser_t* self, dstream_t* ds,
                           const u8* header, usize header_len,
                           const u8* trailer, usize trailer_len,
                           usize max_frame_len);

/**
 * @brief 尝试获取一个完整数据帧
 * @param self    解析器对象
 * @param out_len 输出帧总长度（含定界符），仅当返回 OK 时有效
 * @return 解析结果码
 * @retval DELIMITER_PARSER_OK         成功找到帧，cursor 已指向帧起始
 * @retval DELIMITER_PARSER_NEED_MORE  数据不足，cursor 未移动，状态机保留现场
 * @retval DELIMITER_PARSER_ERR_FRAME_TOO_LONG 帧超长，已跳过超长帧的起始字节，状态机已重置
 * @note 成功找到帧后，调用者可通过 dstream_peek(ds, 0, buf, len) 读取数据，
 *       然后调用 delimiter_parser_consume() 消费该帧
 */
delimiter_parser_result_t delimiter_parser_get_frame(delimiter_parser_t* self, usize* out_len);

/**
 * @brief 消费当前帧，移动 cursor 到帧之后，并重置状态机准备下一帧
 * @param self 解析器对象
 * @pre get_frame 返回 OK 后调用
 */
void delimiter_parser_consume(delimiter_parser_t* self);

/**
 * @brief 重置解析器状态（例如数据流重置或错误恢复）
 * @param self 解析器对象
 */
void delimiter_parser_reset(delimiter_parser_t* self);

#ifdef __cplusplus
}
#endif

#endif // LIBCA_EM_DSTREAM_DELIMITER_PARSER_H
