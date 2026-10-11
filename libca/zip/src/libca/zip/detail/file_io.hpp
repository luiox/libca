#pragma once

#include <cstdio>
#include <string>

#include "libca/core/datatype.hpp"

namespace ca::zip {

/// @brief 以 UTF-8 路径打开 C stdio 文件句柄。
///
/// Windows 的窄字符 fopen 按 ANSI 代码页解释路径，UTF-8 中文/emoji 路径必然打开
/// 失败；这里经 ca::str::OsString（str 模块的编码边界）转 UTF-16 后走 _wfopen，
/// 与 fs::Path::native() 语义一致。POSIX 原生路径即字节序列，直接 fopen。
/// @param path UTF-8 编码的文件路径（非法序列以 U+FFFD 替代，不抛异常）。
/// @param mode 打开模式串（"rb"/"wb" 等，仅 ASCII）。
/// @return 成功返回 stdio 句柄；失败返回 nullptr，由调用方映射 ZipError。
std::FILE* open_stdio_file(const std::string& path, const char* mode);

}   // namespace ca::zip
