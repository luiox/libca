#pragma once

#include "libca/io/error.hpp"

namespace ca::io {

/// @brief SeekFrom 使用的定位基准。
enum class SeekOrigin
{
    Start,
    Current,
    End
};

/// @brief 表示从流起点、当前位置或末尾计算的 seek 位置。
class SeekFrom
{
public:
    /// @brief 从流起点开始的无符号绝对位置。
    static SeekFrom start(u64 position) noexcept;

    /// @brief 相对当前位置的有符号偏移。
    static SeekFrom current(i64 offset) noexcept;

    /// @brief 相对流末尾的有符号偏移。
    static SeekFrom end(i64 offset) noexcept;

    /// @brief 返回定位基准。
    SeekOrigin origin() const noexcept;

    /// @brief 返回 Start 的绝对位置；其它基准返回 0。
    u64 absolute_position() const noexcept;

    /// @brief 返回 Current/End 的相对偏移；Start 返回 0。
    i64 relative_offset() const noexcept;

private:
    SeekFrom(SeekOrigin origin, u64 absolute_position, i64 relative_offset) noexcept;

    SeekOrigin origin_{SeekOrigin::Start};
    u64        absolute_position_{0};
    i64        relative_offset_{0};
};

/// @brief 可随机定位字节流的能力接口。
class Seek
{
public:
    virtual ~Seek() = default;

    /// @brief 定位并返回从流起点计算的新位置。
    virtual IoResult<u64> seek(const SeekFrom& position) = 0;

    /// @brief 返回当前位置。
    IoResult<u64> stream_position();

    /// @brief 定位到流起点。
    IoResult<void> rewind();
};

}   // namespace ca::io
