#pragma once

#include <cstddef>
#include <string>

namespace ca::crypto {

/// @brief 哈希算法抽象基类，统一一次性计算与增量更新两种用法。
/// @note 子类（如 MD5、SHA1、SHA256）实现具体算法。一次性调用用 operator()，
///       分块/流式更新用 add() 累积 + get_hash() 取结果，reset() 复用实例。
class Hash {
public:
    virtual ~Hash() = default;
    /// 一次性计算并返回十六进制 digest 字符串。
    virtual std::string operator()(const void* data, size_t num_bytes) = 0;
    /// 一次性计算（string 重载）。
    virtual std::string operator()(const std::string& text) = 0;
    /// 增量追加数据，不产出结果。
    virtual void add(const void* data, size_t num_bytes) = 0;
    /// 取当前累积数据的十六进制 digest（不重置内部状态）。
    virtual std::string get_hash() = 0;
    /// 取原始字节 digest 写入 buffer（长度由子类 HashBytes 决定）。
    virtual void get_hash(unsigned char buffer[]) = 0;
    /// 清空内部状态，实例可复用。
    virtual void reset() = 0;
};

}  // namespace ca::crypto
