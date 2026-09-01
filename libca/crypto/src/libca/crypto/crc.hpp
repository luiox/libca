///
/// @brief CRC 校验工具
/// @author Canrad
/// @date 2026/05/31
/// @note 命名空间 ca::crypto，提供 CRC16/CRC32 等常用校验
///

#pragma once

#include <cstddef>
#include <cstdint>
#include <string>

namespace ca::crypto {

/// CRC16 校验（CCITT 标准，多项式 0x1021）
struct Crc16
{
    Crc16() = delete;

    /// 计算 CRC16
    static uint16_t calculate(const void* data, size_t len, uint16_t crc = 0);

    /// 计算 C 字符串的 CRC16
    static uint16_t calculate(const char* str);

    /// 计算 std::string 的 CRC16
    static uint16_t calculate(const std::string& s);
};

}   // namespace ca::crypto
