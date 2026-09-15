#pragma once

#include <memory>
#include <vector>

#include "libca/core/datatype.hpp"

namespace ca::zip {

/// @brief RFC 1952 gzip 数据流式解压读取器。
///
/// 按 flag 处理 FTEXT/FHCRC/FEXTRA/FNAME/FCOMMENT 全部头部可选字段（FHCRC
/// 做头 CRC16 校验）；一个成员解压完成后若输入还有剩余则自动继续解析下一
/// 成员（多成员拼接流）。成员尾部 CRC32 与 ISIZE 校验失败、magic 错误、
/// 流截断均抛 std::runtime_error。
class GzipReader {
public:
    /// @brief 接管整段 gzip 数据（可含多个拼接成员）。
    explicit GzipReader(std::vector<ca::u8> data);

    /// @brief 从字节区间构造（内部拷贝一份）。
    GzipReader(const ca::u8* data, ca::usize size);

    ~GzipReader();

    GzipReader(const GzipReader&)            = delete;
    GzipReader& operator=(const GzipReader&) = delete;

    /// @brief 读取解压数据，返回实际字节数；全部成员耗尽返回 0。
    int read(ca::u8* buffer, ca::usize size);

    /// @brief 解压全部成员并返回拼接结果。
    std::vector<ca::u8> read_all();

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

/// @brief 单次解压完整 gzip 数据（支持多成员拼接流）。
/// @return 全部成员未压缩内容的顺序拼接。
/// @throws std::runtime_error magic 错误、不支持的方法、流截断、
///                            保留 flag 置位或尾部 CRC32/ISIZE 校验失败。
std::vector<ca::u8> gzip_decompress(const std::vector<ca::u8>& data);

/// @brief 同上，针对裸字节区间。
std::vector<ca::u8> gzip_decompress(const ca::u8* data, ca::usize size);

}   // namespace ca::zip
