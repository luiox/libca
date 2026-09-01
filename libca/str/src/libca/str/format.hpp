/// @file format.hpp
/// @brief 格式化门面：基于 fmt 的 `{}`-style 格式化，返回 Utf8String 或追加到 builder/std::string。
/// @author Canrad
/// @date 2026/08/09
/// @note 对标 Rust `format!` / `format_args!`。fmt 在本模块以 public 依赖提供，
///       下游模块通过 `add_deps("libca_str")` 间接拿到 fmt，不必各自声明。
///       门面只依赖 `<fmt/core.h>`（与 libca.log 一致），刻意不引入 `<fmt/format.h>`
///       以保持依赖面最小。门面函数都是 inline 模板，实现留在头文件（符合 spec:36 模板条款）。
/// @par UTF-8 校验契约
///      `format` / `format_runtime` 返回 Utf8String，构造时校验 UTF-8；参数产出非法字节时抛
///      `std::runtime_error`，与 `Utf8String(const u8*, usize)` 既有契约一致。
///      `format_to(Utf8StringBuilder&)` 追加时不立即校验，最终 `build()` 时统一校验。
///      `format_to(std::string&)` 不校验 UTF-8，给日志后端/协议代码等字节级场景使用。
/// @code
///   auto msg = ca::str::format("port={}", 8080);      // → Utf8String
///   ca::str::Utf8StringBuilder b;
///   ca::str::format_to(b, "a={}, ", 1);
///   ca::str::format_to(b, "b={}", 2);
///   auto s = b.build();                                // → "a=1, b=2"
/// @endcode

#pragma once

#include "libca/core/datatype.hpp"
#include "utf8_string.hpp"

#include <iterator>
#include <string>
#include <utility>

#include <fmt/core.h>

namespace ca::str {

// ============================================================================
// 编译期校验的格式化
// ============================================================================

/// @brief 编译期校验的 `{}`-style 格式化，返回 Utf8String。
/// @param fmt_str 格式串（字面量，编译期校验占位符数量/类型与 args 一致）。
/// @param args    被格式化的参数。
/// @return 拥有所有权、已校验 UTF-8 的字符串。
/// @throw std::runtime_error 格式化结果含非法 UTF-8 字节时（与 Utf8String 构造一致）。
/// @note 对标 Rust `format!`。模板实现留头文件（spec:36）。
template<typename... Args>
inline Utf8String format(fmt::format_string<Args...> fmt_str, Args&&... args)
{
    std::string tmp;
    // make_format_args 要求实参为左值（fmt 10.x+ 禁止临时量，见 fmt issue #3589）；
    // 函数参数 args... 本身是左值，直接传入即可，不要 std::forward（会得到右值引用）。
    fmt::vformat_to(std::back_inserter(tmp), fmt_str, fmt::make_format_args(args...));
    return Utf8String(reinterpret_cast<const u8*>(tmp.data()), tmp.size());
}

/// @brief 编译期校验的 `{}`-style 格式化，返回 std::string（不校验 UTF-8）。
/// @param fmt_str 格式串（字面量，编译期校验）。
/// @param args    被格式化的参数。
/// @return 格式化后的 std::string。
/// @note 给 std::string 世界（opt/io/env 等模块内部）使用；不校验 UTF-8。
///       若需 UTF-8 保证，用返回 Utf8String 的 format()。
///       命名用 format_std 与返回 Utf8String 的 format 区分，避免重载二义。
template<typename... Args>
inline std::string format_std(fmt::format_string<Args...> fmt_str, Args&&... args)
{
    std::string out;
    fmt::vformat_to(std::back_inserter(out), fmt_str, fmt::make_format_args(args...));
    return out;
}

/// @brief 追加格式化到 Utf8StringBuilder（不立即校验，build() 时统一校验）。
/// @param out     目标 builder，原内容保留，结果追加到末尾。
/// @param fmt_str 格式串（编译期校验）。
/// @param args    被格式化的参数。
/// @return out 的引用（支持链式）。
template<typename... Args>
inline Utf8StringBuilder& format_to(Utf8StringBuilder& out, fmt::format_string<Args...> fmt_str,
                                    Args&&... args)
{
    std::string tmp;
    fmt::vformat_to(std::back_inserter(tmp), fmt_str, fmt::make_format_args(args...));
    out.append(tmp.data(), tmp.size());
    return out;
}

/// @brief 追加格式化到 std::string（不校验 UTF-8，给日志后端/协议代码等字节级场景）。
/// @param out     目标 string，原内容保留，结果追加到末尾。
/// @param fmt_str 格式串（编译期校验）。
/// @param args    被格式化的参数。
/// @return out 的引用（支持链式）。
/// @note 此重载不校验 UTF-8，因为 std::string 在 libca 之外不强制 UTF-8 语义；
///       需 UTF-8 保证请用返回 Utf8String 的 format() 或 format_to(Utf8StringBuilder)。
template<typename... Args>
inline std::string& format_to(std::string& out, fmt::format_string<Args...> fmt_str, Args&&... args)
{
    fmt::vformat_to(std::back_inserter(out), fmt_str, fmt::make_format_args(args...));
    return out;
}

// ============================================================================
// 运行期格式串（无编译期校验）
// ============================================================================

/// @brief 运行期格式串格式化（无编译期校验），用于动态生成的格式串。
/// @param fmt_str 运行期格式串（如从配置/网络读取，无法在编译期校验）。
/// @param args    已通过 fmt::make_format_args 构造的参数 view。
/// @return 拥有所有权、已校验 UTF-8 的字符串。
/// @throw std::runtime_error 格式串非法或结果含非法 UTF-8 字节时。
/// @note 命名用 format_runtime 而非 vformat：fmt 自身导出同签名的 fmt::vformat，
///       若本门面也叫 vformat，`using namespace ca::str` 后调用会与 fmt::vformat
///       经 ADL 产生二义。format_runtime 避开此冲突。实现在 format.cpp。
Utf8String format_runtime(fmt::string_view fmt_str, fmt::format_args args);

}   // namespace ca::str

// ============================================================================
// fmt::formatter 特化：让 Utf8String / Utf8StringRef 可作为 fmt 参数。
// fmt 12.x 不再自动识别 operator string_view() 的隐式转换，须显式特化。
// 两者都有 operator std::string_view()（零拷贝），转发到 string_view 的 formatter 即可。
// 特化须放在 namespace fmt（标准库/第三方类型特化的合法位置）。
// ============================================================================
namespace fmt {

template<>
struct formatter<ca::str::Utf8String> : formatter<std::string_view>
{
    template<typename FormatContext>
    auto format(const ca::str::Utf8String& s, FormatContext& ctx) const -> decltype(ctx.out())
    {
        return formatter<std::string_view>::format(static_cast<std::string_view>(s), ctx);
    }
};

template<>
struct formatter<ca::str::Utf8StringRef> : formatter<std::string_view>
{
    template<typename FormatContext>
    auto format(const ca::str::Utf8StringRef& s, FormatContext& ctx) const -> decltype(ctx.out())
    {
        return formatter<std::string_view>::format(static_cast<std::string_view>(s), ctx);
    }
};

}   // namespace fmt
