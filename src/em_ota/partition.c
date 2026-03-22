#include "partition.h"
#include <em_base/debug.h>
#include <em_base/string_util.h>

/* ============================================================================
 * Port 管理
 * ============================================================================ */

static const partition_port_t *g_partition_port = NULL;

void partition_register_port(const partition_port_t *port)
{
    g_partition_port = port;
}

bool partition_port_is_registered(void)
{
    return g_partition_port != NULL;
}

/* ============================================================================
 * 分区查找
 * ============================================================================ */

const partition_t* partition_find(const partition_t *table, usize count, const char *name)
{
    if (table == NULL || name == NULL) {
        return NULL;
    }

    for (usize i = 0; i < count; i++) {
        if (str_is_equal(table[i].name, name)) {
            return &table[i];
        }
    }
    return NULL;
}

/* ============================================================================
 * 基础操作
 * ============================================================================ */

i32 partition_read(const partition_t *part, u32 offset, u8 *buf, u32 len)
{
    if (part == NULL || buf == NULL) {
        return PARTITION_ERR_INVALID_PARAM;
    }

    if (!partition_port_is_registered()) {
        return PARTITION_ERR_PORT_NOT_SET;
    }

    /* 检查可读属性 */
    if (!(part->flags & PARTITION_FLAG_READABLE)) {
        return PARTITION_ERR_NOT_READABLE;
    }

    /* 检查范围 */
    if (offset >= part->size || len > part->size - offset) {
        return PARTITION_ERR_OUT_OF_RANGE;
    }

    /* 空读取 */
    if (len == 0) {
        return PARTITION_OK;
    }

    /* 调用底层读取 */
    i32 ret = g_partition_port->read(part->start + offset, buf, len);
    if (ret < 0) {
        return PARTITION_ERR_READ_FAIL;
    }

    return PARTITION_OK;
}

i32 partition_write(const partition_t *part, u32 offset, const u8 *data, u32 len)
{
    if (part == NULL || data == NULL) {
        return PARTITION_ERR_INVALID_PARAM;
    }

    if (!partition_port_is_registered()) {
        return PARTITION_ERR_PORT_NOT_SET;
    }

    /* 检查可写属性 */
    if (!(part->flags & PARTITION_FLAG_WRITABLE)) {
        return PARTITION_ERR_READONLY;
    }

    /* 检查范围 */
    if (offset >= part->size || len > part->size - offset) {
        return PARTITION_ERR_OUT_OF_RANGE;
    }

    /* 空写入 */
    if (len == 0) {
        return PARTITION_OK;
    }

    /* 调用底层写入 */
    i32 ret = g_partition_port->write(part->start + offset, data, len);
    if (ret < 0) {
        return PARTITION_ERR_WRITE_FAIL;
    }

    return PARTITION_OK;
}

i32 partition_erase(const partition_t *part)
{
    if (part == NULL) {
        return PARTITION_ERR_INVALID_PARAM;
    }

    if (!partition_port_is_registered()) {
        return PARTITION_ERR_PORT_NOT_SET;
    }

    /* 检查可擦除属性 */
    if (!(part->flags & PARTITION_FLAG_ERASEABLE)) {
        return PARTITION_ERR_READONLY;
    }

    /* 调用底层擦除 */
    i32 ret = g_partition_port->erase(part->start, part->size);
    if (ret < 0) {
        return PARTITION_ERR_ERASE_FAIL;
    }

    return PARTITION_OK;
}

i32 partition_erase_range(const partition_t *part, u32 offset, u32 len)
{
    if (part == NULL) {
        return PARTITION_ERR_INVALID_PARAM;
    }

    if (!partition_port_is_registered()) {
        return PARTITION_ERR_PORT_NOT_SET;
    }

    /* 检查可擦除属性 */
    if (!(part->flags & PARTITION_FLAG_ERASEABLE)) {
        return PARTITION_ERR_READONLY;
    }

    /* 检查范围 */
    if (offset >= part->size || len > part->size - offset) {
        return PARTITION_ERR_OUT_OF_RANGE;
    }

    /* 空擦除 */
    if (len == 0) {
        return PARTITION_OK;
    }

    /* 调用底层擦除 */
    i32 ret = g_partition_port->erase(part->start + offset, len);
    if (ret < 0) {
        return PARTITION_ERR_ERASE_FAIL;
    }

    return PARTITION_OK;
}

/* ============================================================================
 * 流式写入
 * ============================================================================ */

i32 partition_stream_open(partition_stream_t *stream, const partition_t *part, u32 total_size)
{
    if (stream == NULL || part == NULL) {
        return PARTITION_ERR_INVALID_PARAM;
    }

    if (!partition_port_is_registered()) {
        return PARTITION_ERR_PORT_NOT_SET;
    }

    /* 检查可写属性 */
    if (!(part->flags & PARTITION_FLAG_WRITABLE)) {
        return PARTITION_ERR_READONLY;
    }

    /* 检查总大小是否超出分区 */
    if (total_size > part->size) {
        return PARTITION_ERR_OUT_OF_RANGE;
    }

    stream->part = part;
    stream->current_offset = 0;
    stream->total_size = total_size;
    stream->written = 0;
    stream->on_block_written = NULL;
    stream->userdata = NULL;

    return PARTITION_OK;
}

i32 partition_stream_write(partition_stream_t *stream, const u8 *data, u32 len)
{
    if (stream == NULL || data == NULL) {
        return PARTITION_ERR_INVALID_PARAM;
    }

    if (stream->part == NULL) {
        return PARTITION_ERR_NOT_OPEN;
    }

    if (!partition_port_is_registered()) {
        return PARTITION_ERR_PORT_NOT_SET;
    }

    /* 空写入 */
    if (len == 0) {
        return PARTITION_OK;
    }

    /* 检查是否会超出预期总大小 */
    if (len > stream->total_size - stream->written) {
        return PARTITION_ERR_OUT_OF_RANGE;
    }

    /* 检查是否会超出分区大小 */
    if (len > stream->part->size - stream->current_offset) {
        return PARTITION_ERR_OUT_OF_RANGE;
    }

    /* 调用底层写入 */
    i32 ret = g_partition_port->write(stream->part->start + stream->current_offset, data, len);
    if (ret < 0) {
        return PARTITION_ERR_WRITE_FAIL;
    }

    /* 更新状态 */
    stream->current_offset += len;
    stream->written += len;

    /* 回调通知 */
    if (stream->on_block_written != NULL) {
        stream->on_block_written(stream->current_offset - len, data, len, stream->userdata);
    }

    return PARTITION_OK;
}

i32 partition_stream_close(partition_stream_t *stream)
{
    if (stream == NULL) {
        return PARTITION_ERR_INVALID_PARAM;
    }

    if (stream->part == NULL) {
        return PARTITION_ERR_NOT_OPEN;
    }

    /* 检查写入大小是否匹配预期 */
    if (stream->written != stream->total_size) {
        debug_print("[partition] stream close: size mismatch, written=%u, expected=%u",
                    stream->written, stream->total_size);
        return PARTITION_ERR_SIZE_MISMATCH;
    }

    /* 清理状态 */
    stream->part = NULL;
    stream->current_offset = 0;

    return PARTITION_OK;
}

u32 partition_stream_offset(const partition_stream_t *stream)
{
    if (stream == NULL) {
        return 0;
    }
    return stream->current_offset;
}

u32 partition_stream_remaining(const partition_stream_t *stream)
{
    if (stream == NULL || stream->part == NULL) {
        return 0;
    }
    return stream->total_size - stream->written;
}

/* ============================================================================
 * 单元测试
 * ============================================================================ */

