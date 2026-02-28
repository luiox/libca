/**
 * @file length_parser.h
 * @author canrad (1517807724@qq.com)
 * @brief 长度前置解析器
 * @version 0.1
 * @date 2026-02-28
 * 
 * @copyright Copyright (c) 2026
 * 
 */
#ifndef LIBCA_EM_DSTREAM_LENGTH_PARSER_H
#define LIBCA_EM_DSTREAM_LENGTH_PARSER_H

#include "../em_base/datatype.h"
#include "dstream.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 长度前置解析器结果码
 */
typedef enum length_parser_result_enum {
    LENGTH_PARSER_OK = 0,               /**< 成功找到完整帧 */
    LENGTH_PARSER_NEED_MORE,            /**< 数据不足，需等待更多数据 */
    LENGTH_PARSER_ERR_INVALID_LEN,      /**< 长度字段值超出 max_frame_len */
    LENGTH_PARSER_ERR_CHECKSUM,         /**< 校验和错误 */
    LENGTH_PARSER_ERR_SYNC,             /**< 帧头不匹配 */
} length_parser_result_t;

/**
 * @brief 校验函数类型枚举
 */
typedef enum length_parser_cksum_type_enum {
    LENGTH_PARSER_CKSUM_NONE = 0,       /**< 无校验 */
    LENGTH_PARSER_CKSUM_U8,             /**< 8位校验和 */
    LENGTH_PARSER_CKSUM_CRC16,          /**< CRC-16 */
    LENGTH_PARSER_CKSUM_CRC32,          /**< CRC-32 */
} length_parser_cksum_type_t;

/**
 * @brief 校验函数联合体
 */
typedef union length_parser_cksum_func_union {
    void* null_fn;                                           /**< 无校验时使用 */
    u8 (*checksum_u8)(const u8* data, usize len, u8 prev);   /**< 8位校验和函数 */
    u16 (*crc16)(const void* data, usize len, u16 prev);     /**< CRC-16函数 */
    u32 (*crc32)(const void* data, usize len, u32 prev);     /**< CRC-32函数 */
} length_parser_cksum_func_t;

typedef struct length_parser length_parser_t;

/**
 * @brief 长度前置解析器状态
 */
typedef enum length_parser_state_enum {
    LENGTH_STATE_IDLE,              /**< 空闲：寻找 Header 或直接解析 Len */
    LENGTH_STATE_LEN_PARTIAL,       /**< 正在读取 Len 字段 */
    LENGTH_STATE_DATA_AND_CKSUM,    /**< 等待数据完整并进行校验 */
} length_parser_state_t;

/**
 * @brief 长度前置解析器
 */
struct length_parser {
    dstream_t* ds;                  /**< 关联的数据流 */

    /* 配置参数 */
    const u8* header;               /**< 可选帧头（NULL 表示无头） */
    usize header_len;               /**< 帧头长度 */
    u8 len_field_size;              /**< 长度字段字节数 (1/2/4) */
    bool len_big_endian;            /**< 长度字段是否大端序 */
    u8 checksum_size;               /**< 校验字段字节数 (0 表示无校验) */
    bool checksum_big_endian;       /**< 校验字段是否大端序 */
    length_parser_cksum_type_t cksum_type;  /**< 校验类型 */
    length_parser_cksum_func_t cksum_func;  /**< 校验函数 */
    u32 cksum_init_val;             /**< 校验初始值 */
    usize max_frame_len;            /**< 数据部分最大允许长度 */

    /* 运行时状态 */
    length_parser_state_t state;    /**< 状态机状态 */
    u8 partial_count;               /**< 已读取的部分字节数（用于 Len 字段） */
    u32 len_accumulator;            /**< 正在累积的长度值 */
    u32 target_len;                 /**< 目标数据长度（已解析出的） */
    u32 calc_checksum;              /**< 实时计算的校验值 */
    u32 expected_checksum;          /**< 流中读取的校验值 */
};

/**
 * @brief 初始化长度前置解析器
 * @param self              解析器对象
 * @param ds                数据流
 * @param header            可选帧头（NULL 表示无头）
 * @param header_len        帧头长度
 * @param len_field_size    长度字段字节数 (1/2/4)
 * @param len_big_endian    长度字段字节序 (true=大端, false=小端)
 * @param checksum_size     校验字段字节数 (0/1/2/4)
 * @param checksum_big_endian 校验字段字节序 (true=大端, false=小端)
 * @param cksum_type        校验类型
 * @param cksum_func        校验函数（无校验时可传 NULL）
 * @param cksum_init_val    校验初始值
 * @param max_frame_len     数据部分最大允许长度
 */
void length_parser_init(length_parser_t* self, dstream_t* ds,
                        const u8* header, usize header_len,
                        u8 len_field_size, bool len_big_endian,
                        u8 checksum_size, bool checksum_big_endian,
                        length_parser_cksum_type_t cksum_type,
                        length_parser_cksum_func_t cksum_func,
                        u32 cksum_init_val,
                        usize max_frame_len);

/**
 * @brief 尝试获取一个完整数据帧
 * @param self    解析器对象
 * @param out_len 输出数据部分长度（不含头部、长度字段和校验）
 * @return 解析结果码
 * @note 关键行为：
 *   - OK：Cursor 指向帧头，用户应尽快调用 consume
 *   - 错误：解析器会自动 Skip 1 字节并重置状态，试图重新同步
 *   - NEED_MORE：Cursor 保持不动，等待数据
 */
length_parser_result_t length_parser_get_frame(length_parser_t* self, usize* out_len);

/**
 * @brief 消费当前帧，移动 cursor 跳过整个帧
 * @param self 解析器对象
 * @note 跳过 Header + Len + Data + Checksum
 */
void length_parser_consume(length_parser_t* self);

/**
 * @brief 重置解析器状态
 * @param self 解析器对象
 */
void length_parser_reset(length_parser_t* self);

#ifdef __cplusplus
}
#endif

#endif // LIBCA_EM_DSTREAM_LENGTH_PARSER_H
