//
// @brief GB18030 内置转换实现（Tier 2 表驱动）。
// @author Canrad
// @date 2026/09/15
//

#include "libca/str/detail/charset_gb18030.hpp"

#include "libca/str/detail/charset_internal.hpp"
#include "libca/str/utf8_util.hpp"

#include "gb18030_tables.inc"

namespace ca::str::detail {

namespace {

using charset_tables::GB18030_DOUBLE_DECODE;
using charset_tables::GB18030_ENCODE_CP;
using charset_tables::GB18030_ENCODE_SEQ;
using charset_tables::GB18030_MAX_POINTER;
using charset_tables::GB18030_RANGE_CP;
using charset_tables::GB18030_RANGE_PTR;

// 表长取自数组本身：生成物重建（如未来换数据源）时无需改这里的常量。
constexpr usize GB18030_RANGE_COUNT  = sizeof(GB18030_RANGE_CP) / sizeof(GB18030_RANGE_CP[0]);
constexpr usize GB18030_ENCODE_COUNT = sizeof(GB18030_ENCODE_CP) / sizeof(GB18030_ENCODE_CP[0]);

// ============================================================================
// 解码：字节序列 → 码点
// ============================================================================

// 四字节指针 → 码点。指针越界或落在保留指针洞时返回 false。
// 洞判据：段内线性插值达到/越过下一段 start_cp（见生成脚本 lookup_cp_by_pointer）。
bool gb18030_pointer_to_cp(u32 pointer, u32& cp_out)
{
    if (pointer > GB18030_MAX_POINTER)
        return false;
    usize lo = 0, hi = GB18030_RANGE_COUNT - 1, ans = GB18030_RANGE_COUNT - 1;
    while (lo <= hi) {
        const usize mid = lo + (hi - lo) / 2;
        if (GB18030_RANGE_PTR[mid] <= pointer) {
            ans = mid;
            lo = mid + 1;
        } else {
            if (mid == 0)
                break;
            hi = mid - 1;
        }
    }
    const u32 cp = GB18030_RANGE_CP[ans] + (pointer - GB18030_RANGE_PTR[ans]);
    if (ans + 1 < GB18030_RANGE_COUNT && cp >= GB18030_RANGE_CP[ans + 1])
        return false;  // 指针洞（BMP 块与增补平面块之间的保留段 [39420, 188999]）
    cp_out = cp;
    return true;
}

// 解码下一个 gb18030 序列：cp_out 输出码点，返回消耗字节数；非法时返回 0 并填 err。
usize gb18030_next_code_point(const u8* data, usize size, usize pos, u32& cp_out,
                              core::Status& err)
{
    const u8 b0 = data[pos];
    if (b0 < 0x80) {  // ASCII 直通
        cp_out = b0;
        return 1;
    }
    if (b0 == 0x80 || b0 == 0xFF) {  // 单字节仅 0x80 / 0xFF 非法（CP_936 的 0x80→€ 口径不采用）
        err = charset_invalid_sequence("gb18030", pos);
        return 0;
    }
    if (pos + 1 >= size) {
        err = charset_invalid_sequence("gb18030", pos);
        return 0;
    }
    const u8 b1 = data[pos + 1];

    if (b1 >= 0x30 && b1 <= 0x39) {  // 四字节：81-FE 30-39 81-FE 30-39
        if (pos + 3 >= size) {
            err = charset_invalid_sequence("gb18030", pos);
            return 0;
        }
        const u8 b2 = data[pos + 2];
        const u8 b3 = data[pos + 3];
        if (b2 < 0x81 || b2 > 0xFE || b3 < 0x30 || b3 > 0x39) {
            err = charset_invalid_sequence("gb18030", pos);
            return 0;
        }
        const u32 pointer = ((b0 - 0x81) * 10 + (b1 - 0x30)) * 1260 + (b2 - 0x81) * 10
                            + (b3 - 0x30);
        if (!gb18030_pointer_to_cp(pointer, cp_out)) {
            err = charset_invalid_sequence("gb18030", pos);
            return 0;
        }
        return 4;
    }

    if ((b1 >= 0x40 && b1 <= 0x7E) || (b1 >= 0x80 && b1 <= 0xFE)) {  // 双字节
        const usize trail_index = (b1 <= 0x7E) ? static_cast<usize>(b1 - 0x40)
                                               : static_cast<usize>(b1 - 0x41);
        cp_out = GB18030_DOUBLE_DECODE[(b0 - 0x81) * 190 + trail_index];
        return 2;
    }

    err = charset_invalid_sequence("gb18030", pos);  // 0x7F、<0x30 等非法 trail
    return 0;
}

// 解码整个输入 → 码点序列（gb18030_to_utf8 / gb18030_to_wide 共用）。
core::StatusResult<std::u32string> gb18030_to_code_points(std::string_view gb18030)
{
    const u8*  data = reinterpret_cast<const u8*>(gb18030.data());
    const usize size = gb18030.size();

    std::u32string cps;
    cps.reserve(size);
    usize pos = 0;
    while (pos < size) {
        u32         cp  = 0;
        core::Status err;
        const usize len = gb18030_next_code_point(data, size, pos, cp, err);
        if (len == 0)
            return core::Err(err);
        cps.push_back(cp);
        pos += len;
    }
    return core::Ok<std::u32string>(std::move(cps));
}

// ============================================================================
// 编码：码点 → 字节序列
// ============================================================================

// 双字节编码表二分（下界）：命中返回表内下标，未命中返回 ENCODE_COUNT。
usize gb18030_encode_lookup(u32 cp)
{
    usize lo = 0, hi = GB18030_ENCODE_COUNT;
    while (lo < hi) {
        const usize mid = lo + (hi - lo) / 2;
        if (GB18030_ENCODE_CP[mid] < cp)
            lo = mid + 1;
        else
            hi = mid;
    }
    if (lo < GB18030_ENCODE_COUNT && GB18030_ENCODE_CP[lo] == cp)
        return lo;
    return GB18030_ENCODE_COUNT;
}

// 码点 → gb18030 字节序列（优先双字节，否则四字节）。不可表示返回 false
// （仅代理项 / >U+10FFFF 会走到这里；合法标量值恒可编码）。
bool gb18030_cp_to_bytes(u32 cp, u8 out[4], usize& len)
{
    if (cp < 0x80) {
        out[0] = static_cast<u8>(cp);
        len = 1;
        return true;
    }
    if (cp > 0x10FFFF || (cp >= 0xD800 && cp <= 0xDFFF))
        return false;

    const usize idx = gb18030_encode_lookup(cp);
    if (idx < GB18030_ENCODE_COUNT) {  // 双字节（含欧元 A2E3、PUA 区段等）
        const u16 seq = GB18030_ENCODE_SEQ[idx];
        out[0] = static_cast<u8>(seq >> 8);
        out[1] = static_cast<u8>(seq & 0xFF);
        len = 2;
        return true;
    }

    // 四字节：区间表二分（greatest k 使 RANGE_CP[k] <= cp）。
    usize lo = 0, hi = GB18030_RANGE_COUNT - 1, ans = GB18030_RANGE_COUNT - 1;
    while (lo <= hi) {
        const usize mid = lo + (hi - lo) / 2;
        if (GB18030_RANGE_CP[mid] <= cp) {
            ans = mid;
            lo = mid + 1;
        } else {
            if (mid == 0)
                break;
            hi = mid - 1;
        }
    }
    const u32 pointer = GB18030_RANGE_PTR[ans] + (cp - GB18030_RANGE_CP[ans]);
    if (pointer > GB18030_MAX_POINTER)
        return false;
    out[0] = static_cast<u8>(0x81 + pointer / 12600);
    out[1] = static_cast<u8>(0x30 + pointer % 12600 / 1260);
    out[2] = static_cast<u8>(0x81 + pointer % 1260 / 10);
    out[3] = static_cast<u8>(0x30 + pointer % 10);
    len = 4;
    return true;
}

}  // namespace

// ============================================================================
// 门面实现：字节 ↔ UTF-8 / wchar
// ============================================================================

core::StatusResult<std::string> gb18030_to_utf8(std::string_view gb18030)
{
    auto cps = gb18030_to_code_points(gb18030);
    if (cps.is_err())
        return core::Err(cps.unwrap_err());

    std::string utf8;
    utf8.reserve(gb18030.size());
    u8 buf[4];
    for (u32 cp : std::move(cps).unwrap()) {
        const usize n = utf8_encode_code_point(cp, buf);
        utf8.append(reinterpret_cast<const char*>(buf), n);
    }
    return core::Ok<std::string>(std::move(utf8));
}

core::StatusResult<std::string> utf8_to_gb18030(std::string_view utf8)
{
    const u8*  data = reinterpret_cast<const u8*>(utf8.data());
    const usize size = utf8.size();

    std::string out;
    out.reserve(size);
    usize pos = 0;
    while (pos < size) {
        u32 cp = 0;
        if (!utf8_next_code_point(data, size, pos, cp))
            return core::Err(charset_invalid_sequence("utf-8", pos));
        u8   buf[4];
        usize len = 0;
        if (!gb18030_cp_to_bytes(cp, buf, len))
            return core::Err(charset_unrepresentable("gb18030", cp));
        out.append(reinterpret_cast<const char*>(buf), len);
    }
    return core::Ok<std::string>(std::move(out));
}

core::StatusResult<std::wstring> gb18030_to_wide(std::string_view gb18030)
{
    auto cps = gb18030_to_code_points(gb18030);
    if (cps.is_err())
        return core::Err(cps.unwrap_err());

    std::wstring wide;
    wide.reserve(gb18030.size());
    for (u32 cp : std::move(cps).unwrap())
        wide_push_code_point(wide, cp);
    return core::Ok<std::wstring>(std::move(wide));
}

core::StatusResult<std::string> wide_to_gb18030(std::wstring_view wide)
{
    auto cps = wide_to_code_points(wide);
    if (cps.is_err())
        return core::Err(cps.unwrap_err());

    std::u32string code_points = std::move(cps).unwrap();
    std::string    out;
    out.reserve(code_points.size() * 2);
    for (u32 cp : code_points) {
        u8   buf[4];
        usize len = 0;
        if (!gb18030_cp_to_bytes(cp, buf, len))
            return core::Err(charset_unrepresentable("gb18030", cp));
        out.append(reinterpret_cast<const char*>(buf), len);
    }
    return core::Ok<std::string>(std::move(out));
}

}  // namespace ca::str::detail
