//
// @brief GB18030 内置转换（Tier 2 表驱动，覆盖 GBK / GB2312）。
// @author Canrad
// @date 2026/09/15
//
// str 模块内部实现，不对外导出；对外一律走 CharsetConverter 门面。
// 码表见 gb18030_tables.inc（tools/gen_charset_tables.py 生成入库），
// 语义对齐 WHATWG encoding 标准 index-gb18030 / python gb18030 codec：
//   - 0x00-0x7F ASCII 直通；单字节 0x80 / 0xFF 非法；
//   - 双字节序列查解码表；四字节序列按线性区间表换算（含指针洞拒绝）；
//   - 编码按 双字节表（二分）→ 四字节区间表（二分）的优先级，任何合法
//     Unicode 标量值都可编码。
//

#pragma once

#include "libca/core/status.hpp"

#include <string>
#include <string_view>

namespace ca::str::detail {

core::StatusResult<std::string> gb18030_to_utf8(std::string_view gb18030);

core::StatusResult<std::string> utf8_to_gb18030(std::string_view utf8);

core::StatusResult<std::wstring> gb18030_to_wide(std::string_view gb18030);

core::StatusResult<std::string> wide_to_gb18030(std::wstring_view wide);

}  // namespace ca::str::detail
