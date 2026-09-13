/// @file bom.hpp
/// @brief BOM（Byte Order Mark，字节顺序标记）检测与剥离工具。仅依赖 datatype.hpp 与
///        <string_view>，不依赖其他字符串类型。
/// @author Canrad
/// @date 2026/09/13

#pragma once

#include "libca/core/datatype.hpp"

#include <string_view>

namespace ca::str {

/// @brief Unicode BOM 类型。
/// @note FF FE（Utf16Le）与 FF FE 00 00（Utf32Le）前缀重叠：detect_bom 按**最长匹配优先**
///       判别，输入不足 4 字节时不会判为 Utf32Le。
enum class BomType {
    None,     ///< 无 BOM
    Utf8,     ///< EF BB BF
    Utf16Le,  ///< FF FE
    Utf16Be,  ///< FE FF
    Utf32Le,  ///< FF FE 00 00
    Utf32Be,  ///< 00 00 FE FF
};

/// @brief 检测字节序列开头的 BOM 类型，最长匹配优先。
/// @param bytes 待检测字节序列（按原始字节看待，不要求是合法 UTF-8）。
/// @return 命中的 BOM 类型；无 BOM 返回 None。
/// @note 输入不足一个完整 BOM 长度时不得误判为更长 BOM：如 FF FE 00（仅 3 字节）
///       判为 Utf16Le 而非 Utf32Le，EF BB（仅 2 字节）返回 None。
BomType detect_bom(std::string_view bytes) noexcept;

/// @brief 返回 BOM 类型对应的字节序列长度。@param type BOM 类型。
/// @return Utf8 为 3，Utf16Le/Utf16Be 为 2，Utf32Le/Utf32Be 为 4，None 为 0。
usize bom_byte_length(BomType type) noexcept;

/// @brief 剥离字节序列开头的 BOM。
/// @param bytes 输入字节序列。
/// @return 剥离 BOM 后的子视图（零拷贝，指向原数据）；无 BOM 时原样返回。
/// @note 返回视图的生命周期与输入底层数据一致，输入数据须在使用期内保持有效。
std::string_view strip_bom(std::string_view bytes) noexcept;

}  // namespace ca::str
