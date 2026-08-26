#pragma once

#include <cstddef>
#include <vector>

#include <libca/core/datatype.hpp>

namespace ca::zip {

/// @brief 裸 Deflate 流（raw deflate，无 zlib/gzip 头）解压。
///
/// @param data 压缩数据指针。
/// @param size 压缩数据字节数。
/// @param hint_uncompressed_size 未压缩大小的可信提示；仅用于输出预分配，
///                               实际产出以流结束为准。0 表示无提示。
/// @return 解压后的完整字节序列。
/// @throws std::runtime_error 数据损坏或流不完整时抛出。
std::vector<ca::u8> inflate_raw(const ca::u8* data, size_t size, size_t hint_uncompressed_size);

}   // namespace ca::zip
