#pragma once

#include <cstddef>

#include "libca/core/datatype.hpp"

namespace ca::zip {

/// @brief CRC32 校验器（zlib 语义：初值 0，增量更新）。
class Crc32 {
public:
    Crc32();

    /// @brief 增量喂入数据。
    void update(const void* data, size_t size);

    /// @brief 取当前校验值。
    ca::u32 value() const;

    /// @brief 重置为初始状态。
    void reset();

private:
    ca::u32 crc_;
};

/// @brief Adler-32 校验器（zlib 语义）。
class Adler32 {
public:
    Adler32();

    /// @brief 增量喂入数据。
    void update(const void* data, size_t size);

    /// @brief 取当前校验值。
    ca::u32 value() const;

    /// @brief 重置为初始状态。
    void reset();

private:
    ca::u32 adler_;
};

}   // namespace ca::zip
