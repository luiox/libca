#pragma once

#include "datatype.hpp"

#include <algorithm>
#include <memory>
#include <stdexcept>

/// @file array_util.hpp
/// @brief 轻量数组工具，给 Java Arrays intrinsic 映射提供稳定入口。

namespace ca::core {

/// @brief Java `Arrays` 常用 intrinsic 的原始数组辅助工具。
///
/// `ArrayUtil` 面向已知长度的连续内存，不拥有输入数组。返回新数组的 API 使用
/// `std::unique_ptr<T[]>` 表达所有权，调用方负责保存返回值。异常只用于 C++ API
/// 的非法参数，例如非空长度搭配空指针或无效范围。
class ArrayUtil
{
public:
    /// @brief 比较两个数组区间的元素内容是否完全相等。
    /// @return 长度不同、非空空指针或任意元素不同都会返回 false。
    template<typename T>
    static bool equals(const T* lhs, ca::usize lhs_len, const T* rhs, ca::usize rhs_len)
    {
        if (lhs_len != rhs_len)
            return false;
        if (lhs_len == 0)
            return true;
        if (lhs == nullptr || rhs == nullptr)
            return false;
        return std::equal(lhs, lhs + lhs_len, rhs);
    }

    /// @brief 用 value 填充 `[data, data + len)` 区间。
    /// @throws std::invalid_argument 当 len 非 0 且 data 为空。
    template<typename T>
    static void fill(T* data, ca::usize len, const T& value)
    {
        if (data == nullptr && len != 0)
            throw std::invalid_argument("ArrayUtil::fill: null data");
        std::fill_n(data, len, value);
    }

    /// @brief 复制数组并调整到 new_len，超出原长度的部分做值初始化。
    /// @return 拥有新数组的 `std::unique_ptr<T[]>`；new_len 为 0 时返回空指针。
    /// @throws std::invalid_argument 当 len 非 0 且 data 为空。
    template<typename T>
    static std::unique_ptr<T[]> copy_of(const T* data, ca::usize len, ca::usize new_len)
    {
        if (data == nullptr && len != 0)
            throw std::invalid_argument("ArrayUtil::copy_of: null data");

        auto       out      = make_array<T>(new_len);
        const auto copy_len = len < new_len ? len : new_len;
        if (copy_len != 0)
            std::copy_n(data, copy_len, out.get());
        return out;
    }

    /// @brief 复制 `[from, to)` 范围并生成长度为 `to - from` 的新数组。
    ///
    /// 如果 `to` 超过输入长度，尾部按 T 的默认值填充，语义接近 Java
    /// `Arrays.copyOfRange`。
    /// @throws std::invalid_argument 当 from > to 或 len 非 0 且 data 为空。
    /// @throws std::out_of_range 当 from 超过输入长度。
    template<typename T>
    static std::unique_ptr<T[]> copy_of_range(const T* data, ca::usize len, ca::usize from,
                                              ca::usize to)
    {
        if (from > to)
            throw std::invalid_argument("ArrayUtil::copy_of_range: invalid range");
        if (from > len)
            throw std::out_of_range("ArrayUtil::copy_of_range: range starts past input");
        if (data == nullptr && len != 0)
            throw std::invalid_argument("ArrayUtil::copy_of_range: null data");

        const auto new_len   = to - from;
        auto       out       = make_array<T>(new_len);
        const auto available = len - from;
        const auto copy_len  = available < new_len ? available : new_len;
        if (copy_len != 0)
            std::copy_n(data + from, copy_len, out.get());
        return out;
    }

private:
    template<typename T>
    static std::unique_ptr<T[]> make_array(ca::usize len)
    {
        if (len == 0)
            return nullptr;
        return std::make_unique<T[]>(len);
    }
};

}   // namespace ca::core

namespace ca {
using core::ArrayUtil;
}
