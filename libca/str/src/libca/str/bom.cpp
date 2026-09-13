#include "bom.hpp"

#include <cstring>
#include <string_view>

namespace ca::str {

namespace {

using namespace std::string_view_literals;

// 各 BOM 的字节序列。UTF-32LE 与 UTF-16LE 前缀重叠（FF FE），检测必须最长匹配优先。
constexpr std::string_view BOM_UTF32LE = "\xFF\xFE\x00\x00"sv;
constexpr std::string_view BOM_UTF32BE = "\x00\x00\xFE\xFF"sv;
constexpr std::string_view BOM_UTF8    = "\xEF\xBB\xBF"sv;
constexpr std::string_view BOM_UTF16LE = "\xFF\xFE"sv;
constexpr std::string_view BOM_UTF16BE = "\xFE\xFF"sv;

// 判断 bytes 开头是否为 prefix。同时校验长度：输入不足 prefix 长度时不匹配，
// 保证「输入不足一个完整 BOM 长度时不得误判」。
bool starts_with_bytes(std::string_view bytes, std::string_view prefix) noexcept
{
    return bytes.size() >= prefix.size() &&
           std::memcmp(bytes.data(), prefix.data(), prefix.size()) == 0;
}

}  // namespace

BomType detect_bom(std::string_view bytes) noexcept
{
    // 4 字节 BOM 先判：FF FE 00 00 覆盖 FF FE 的前缀重叠段，最长匹配优先才不会把
    // UTF-32LE 文件误判成 UTF-16LE。长度校验在 starts_with_bytes 内完成。
    if (starts_with_bytes(bytes, BOM_UTF32LE)) {
        return BomType::Utf32Le;
    }
    if (starts_with_bytes(bytes, BOM_UTF32BE)) {
        return BomType::Utf32Be;
    }
    if (starts_with_bytes(bytes, BOM_UTF8)) {
        return BomType::Utf8;
    }
    if (starts_with_bytes(bytes, BOM_UTF16LE)) {
        return BomType::Utf16Le;
    }
    if (starts_with_bytes(bytes, BOM_UTF16BE)) {
        return BomType::Utf16Be;
    }
    return BomType::None;
}

usize bom_byte_length(BomType type) noexcept
{
    switch (type) {
        case BomType::Utf16Le:
        case BomType::Utf16Be:
            return 2;
        case BomType::Utf8:
            return 3;
        case BomType::Utf32Le:
        case BomType::Utf32Be:
            return 4;
        case BomType::None:
        default:
            return 0;
    }
}

std::string_view strip_bom(std::string_view bytes) noexcept
{
    const BomType type = detect_bom(bytes);
    if (type == BomType::None) {
        return bytes;
    }
    bytes.remove_prefix(bom_byte_length(type));
    return bytes;
}

}  // namespace ca::str
