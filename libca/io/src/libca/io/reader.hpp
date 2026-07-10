#pragma once

#include "libca/core/bytes.hpp"
#include "libca/io/error.hpp"

namespace ca::io {

/// @brief 同步可读字节流接口，统一文件、管道和 socket 的 EOF 与短读语义。
///
/// read() 成功返回实际读取字节数，返回 0 表示 EOF。实现允许短读；非阻塞资源暂时无数据
/// 必须返回 WouldBlock，而不是返回 0。
class Reader
{
public:
    virtual ~Reader() = default;

    /// @brief 最多读取 capacity 个字节。
    virtual IoResult<usize> read(u8* buffer, usize capacity) = 0;

    /// @brief 读取恰好 length 个字节；提前 EOF 返回 UnexpectedEof。
    IoResult<void> read_exact(u8* buffer, usize length);

    /// @brief 读取到 EOF，若输入超过 max_length 则返回 InvalidData。
    IoResult<ca::core::Bytes> read_to_end(usize max_length);
};

}   // namespace ca::io
