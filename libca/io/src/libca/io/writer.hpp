#pragma once

#include "libca/core/bytes.hpp"
#include "libca/io/error.hpp"

namespace ca::io {

/// @brief 同步可写字节流接口，统一文件、管道和 socket 的短写与 flush 语义。
class Writer
{
public:
    virtual ~Writer() = default;

    /// @brief 尝试写入 length 个字节，成功返回实际写入长度。
    virtual IoResult<usize> write(const u8* data, usize length) = 0;

    /// @brief 把本 Writer 的缓冲交给下一层，不承诺持久化到存储介质。
    virtual IoResult<void> flush() = 0;

    /// @brief 处理短写并写入全部数据；非空写入返回 0 时报告 WriteZero。
    IoResult<void> write_all(const u8* data, usize length);

    /// @brief ByteSlice 便捷重载。
    IoResult<void> write_all(ca::core::ByteSlice data);
};

}   // namespace ca::io
