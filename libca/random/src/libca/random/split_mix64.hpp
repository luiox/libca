#pragma once

#include <cassert>
#include <cstdint>
#include <stdexcept>

#include "libca/core/datatype.hpp"

namespace ca::random {

/// @brief SplitMix64 确定性伪随机数生成器（header-only，同 seed 同序列）。
/// @note 收敛基准 = mj2x vmc4 版内联实现（mj2x/backend/vmc4/vmc4_instance.cpp 的
///       SplitMix64，luiox/morpher#1027 A3 收敛来源）：
///       `state_ += 0x9E3779B97F4A7C15; z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9;
///       z = (z ^ (z >> 27)) * 0x94D049BB133111EB; return z ^ (z >> 31);`
///       逐位锁定，morpher 收敛后要求输出逐位不变；任何常数/移位/顺序调整都属
///       不兼容变更，必须与 morpher 侧同步。
class SplitMix64 {
public:
    /// @brief 以 seed 初始化内部状态。
    /// @note seed 本身不直接产出：首步 next() 先做黄金比例增量再混合。
    explicit SplitMix64(u64 seed)
        : state_(seed)
    {}

    /// @brief 推进一步并返回 64 位伪随机数。
    u64 next()
    {
        // 以下四步与收敛基准逐位一致（含常数与移位方向），勿改动。
        state_ += 0x9E3779B97F4A7C15ULL;
        u64 z = state_;
        z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ULL;
        z = (z ^ (z >> 27)) * 0x94D049BB133111EBULL;
        return z ^ (z >> 31);
    }

    /// @brief 产出 [0, bound) 内的伪随机数（next() % bound，与正典一致）。
    /// @throws std::invalid_argument bound == 0 时抛出（Release 构建下断言被剥离，
    ///         异常兜底；正典对 bound == 0 是 UB，这里按模块惯例显式拒绝）。
    /// @note 与正典一致直接取模：bound 非 2 的幂时有轻微模偏差（只影响分布形状，
    ///       不影响可复现性）。
    u64 next_bounded(u64 bound)
    {
        assert(bound > 0 && "ca::random::SplitMix64::next_bounded: bound must be > 0");
        if (bound == 0)
            throw std::invalid_argument("ca::random::SplitMix64::next_bounded: bound must be > 0");
        return next() % bound;
    }

private:
    u64 state_;
};

}  // namespace ca::random
