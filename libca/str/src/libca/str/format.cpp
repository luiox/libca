//
// @brief 格式化门面实现
// @author Canrad
// @date 2026/08/09
//
// 仅 format_runtime 的非模板实现在此；format/format_to 是模板，已留在 format.hpp。
//

#include "format.hpp"

#include <iterator>
#include <string>

namespace ca::str {

Utf8String format_runtime(fmt::string_view fmt_str, fmt::format_args args)
{
    std::string tmp;
    fmt::vformat_to(std::back_inserter(tmp), fmt_str, args);
    return Utf8String(reinterpret_cast<const u8*>(tmp.data()), tmp.size());
}

}  // namespace ca::str
