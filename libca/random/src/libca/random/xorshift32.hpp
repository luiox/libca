#pragma once

#include <cassert>
#include <cstdint>
#include <stdexcept>

#include "libca/core/datatype.hpp"

namespace ca::random {

/// @brief Xorshift32 确定性伪随机数生成器（header-only，同 seed 同序列）。
/// @note 收敛基准 = mj2x regc 版（mj2x/backend/regc/regc_cache_pool.cpp 的
///       regcStringXorshift32，luiox/morpher#1027 A4 收敛来源），与同仓
///       mj2x/core/native_lib_crypto.hpp 的 xorshift32Step（vmc4 链路引用）
///       两处正典同式：`x ^= x << 13; x ^= x >> 17; x ^= x << 5;`
///       u32 回绕即 32 位掩码语义，逐位锁定；任何调整都属不兼容变更，
///       必须与 morpher 侧同步。
/// @note seed == 0 是定义行为下的死锁态：序列恒为 0（正典亦然），调用方
///       需自行保证种子非零（morpher 侧各链路均有 0 回退固定种子的约定）。
class Xorshift32 {
public:
    /// @brief 以 seed 初始化内部状态。
    /// @note seed == 0 合法但产出恒 0 序列（见类注释），不做隐式替换——
    ///       与正典一致，回退策略归调用方。
    explicit Xorshift32(u32 seed)
        : state_(seed)
    {}

    /// @brief 推进一步并返回 32 位伪随机数（推进后的完整状态，与正典
    ///        step 函数同值：返回值即新状态本身）。
    u32 next()
    {
        // 以下三步与收敛基准逐位一致（含移位方向与顺序），勿改动。
        u32 x = state_;
        x ^= x << 13;
        x ^= x >> 17;
        x ^= x << 5;
        state_ = x;
        return x;
    }

    /// @brief 产出 [0, bound) 内的伪随机数（next() % bound）。
    /// @throws std::invalid_argument bound == 0 时抛出（Release 构建下断言被
    ///         剥离，异常兜底）。
    /// @note 直接取模，bound 非 2 的幂时有轻微模偏差（只影响分布形状，
    ///       不影响可复现性）。
    u32 next_bounded(u32 bound)
    {
        assert(bound > 0 && "ca::random::Xorshift32::next_bounded: bound must be > 0");
        if (bound == 0)
            throw std::invalid_argument("ca::random::Xorshift32::next_bounded: bound must be > 0");
        return next() % bound;
    }

private:
    u32 state_;
};

}  // namespace ca::random
