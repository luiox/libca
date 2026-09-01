#pragma once

#include <iterator>
#include <string>
#include <string_view>
#include <utility>

#include <fmt/core.h>

#include "libca/log/logger.hpp"

/// @file fmt_format.hpp
/// @brief 门面侧：把 fmt 格式串 + 参数包装成 OpaqueFormat。**仅门面侧使用，后端不 include**。
/// @note FmtArgsHolder 是 view：fmt::format_args 指向调用方栈帧上的实参，生命周期不超过
///       单次 log 调用。同步调用 backend->log() 安全；严禁跨线程入队。

namespace ca::log::detail {

/// @brief 持有 fmt 格式串 + format_args 的 OpaqueFormat 实现。
class FmtArgsHolder final : public OpaqueFormat
{
public:
    /// @param fmt_str 格式串（view）。
    /// @param args    fmt::format_args（view，指向调用方栈帧实参）。
    FmtArgsHolder(fmt::string_view fmt_str, fmt::format_args args) noexcept
        : fmt_str_(fmt_str)
        , args_(args)
    {}

    void render_to(std::string& out) const override
    {
        fmt::vformat_to(std::back_inserter(out), fmt_str_, args_);
    }

private:
    fmt::string_view fmt_str_;
    fmt::format_args args_;
};

}   // namespace ca::log::detail
