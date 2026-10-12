#pragma once

#include <cstddef>
#include <string>

#include "libca/core/datatype.hpp"

namespace ca::crypto {

/// @brief SHA-3（Keccak）摘要，遵循 NIST FIPS 202。
///
/// @note 实现依据 FIPS 202 公开规范（Keccak-f[1600] 置换、ρ 偏移表、ι 轮常数），
///       正确性由 NIST 官方向量单测锁定（见 `unittest/sha3_test.cpp`）。
///       本文件为 libca 自有实现，不含任何第三方源码。
///
/// @par 用法
/// @code
///   SHA3 h;                       // 默认 SHA3-256
///   auto d1 = h("Hello World");   // 一次性
///   auto d2 = h(p, n);            // 任意字节块
///
///   SHA3 s;                       // 流式：可任意分段 add，最后 get_hash()
///   s.add(p1, n1);
///   s.add(p2, n2);
///   auto d3 = s.get_hash();
/// @endcode
///
/// @par 与其他摘要类的一致性
/// 本类提供 `operator()` / `add()` / `get_hash()` / `reset()` 四件套，与 MD5 /
/// SHA1 / SHA256 的交互形态对齐；`get_hash()` 不破坏内部状态，可重复取摘要。
///
/// @note 摘要以小写十六进制字符串返回（224/256/384/512 位分别对应
///       56/64/96/128 个十六进制字符）。
class SHA3 {
public:
    /// @brief 摘要变体。
    enum Bits { Bits224 = 224, Bits256 = 256, Bits384 = 384, Bits512 = 512 };

    /// @brief 构造并初始化为全零状态。
    /// @param bits 摘要长度，默认 256 位。
    explicit SHA3(Bits bits = Bits256);

    /// @brief 重置为初始状态（等价于重新构造）。
    void reset();

    /// @brief 追加任意长度数据，可跨块边界分段调用。
    /// @param data       数据首地址，可为任意对齐；为 `nullptr` 时无操作。
    /// @param num_bytes  字节数；为 0 时无操作。
    void add(const void* data, ca::usize num_bytes);

    /// @brief 取当前内容的摘要，不改变内部状态，可重复调用。
    /// @return 小写十六进制字符串。
    std::string get_hash();

    /// @brief 重置后计算一段数据的摘要。
    std::string operator()(const void* data, ca::usize num_bytes);

    /// @brief 重置后计算一个字符串的摘要。
    std::string operator()(const std::string& text);

private:
    /// @brief 吸收一个完整 rate 块（调用方保证已凑满 `m_rate` 字节）。
    /// @param block 指向 `m_rate` 字节的块首地址，可为任意对齐。
    void absorb_block(const ca::u8* block);

    /// @brief 施加 FIPS 202 §3.3 的 Keccak-f[1600] 置换。
    void permute();

    /// @brief 施加填充并吸收末块，然后取前 `m_bits/8` 字节为摘要。
    /// @note 不修改内部状态（调用方负责保存/恢复）。
    void squeeze_into(ca::u8* out) const;

    /// Keccak 状态：5×5 个 64 位 lane，共 1600 位。线性索引 = x + 5*y。
    ca::u64 m_state[25];

    /// 未吸收的残余字节（< `m_rate`），用于跨 `add()` 调用拼接。
    ca::u8 m_buffer[144];

    /// rate 字节数：SHA3-224=144、256=136、384=104、512=72。
    ca::usize m_rate;

    /// 摘要字节数：`m_bits / 8`。
    ca::usize m_digest_size;

    /// 当前已缓冲、尚未吸收的字节数（< `m_rate`）。
    ca::usize m_buffer_size;

    /// 变体；构造时定下 m_rate/m_digest_size，运算中不再改变。
    Bits m_bits;
};

}  // namespace ca::crypto