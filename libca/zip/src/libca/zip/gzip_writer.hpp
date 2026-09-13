#pragma once

#include <memory>
#include <vector>

#include "libca/core/datatype.hpp"

namespace ca::zip {

/// @brief 默认压缩级别（zlib 语义，-1 等价 Z_DEFAULT_COMPRESSION）。
inline constexpr int kGzipDefaultLevel = -1;

/// @brief RFC 1952 gzip 流式压缩写出器（内存汇聚）。
///
/// 构造即写入 10 字节 gzip 头（MTIME = 0，保证相同输入产出确定字节序列）；
/// write() 增量压缩，finish() 结束 deflate 流并追加 CRC32 与 ISIZE 尾部。
/// 完成后经 output()/take() 取得完整 gzip 字节。
class GzipWriter {
public:
    /// @brief 以默认压缩级别创建。
    GzipWriter();

    /// @brief 指定压缩级别（-1 默认 / 0-9）；非法值抛 std::runtime_error。
    explicit GzipWriter(int level);

    /// @brief 若尚未 finish 则先收尾再释放资源。
    ~GzipWriter();

    GzipWriter(const GzipWriter&)            = delete;
    GzipWriter& operator=(const GzipWriter&) = delete;

    /// @brief 压缩并暂存一段原始数据；finish 之后调用抛 std::runtime_error。
    void write(const ca::u8* data, ca::usize size);

    /// @brief 同上。
    void write(const std::vector<ca::u8>& data);

    /// @brief 收尾：结束 deflate 流并写 CRC32 与 ISIZE 尾部；重复调用无害。
    void finish();

    /// @brief 是否已完成收尾。
    bool finished() const;

    /// @brief 已生成的完整 gzip 字节（含头尾）。
    const std::vector<ca::u8>& output() const;

    /// @brief 取走生成的 gzip 字节并清空内部缓冲。
    std::vector<ca::u8> take();

private:
    void init_impl(int level);

    struct Impl;
    std::unique_ptr<Impl> impl_;
};

/// @brief 单次压缩为 gzip 格式（RFC 1952）。
/// @param data  原始数据。
/// @param level 压缩级别（-1 默认 / 0-9）；非法值抛 std::runtime_error。
/// @return 完整 gzip 字节序列（头 + deflate 数据 + CRC32/ISIZE 尾）。
/// @throws std::runtime_error 级别非法或 zlib 内部错误。
std::vector<ca::u8> gzip_compress(const std::vector<ca::u8>& data,
                                  int                        level = kGzipDefaultLevel);

/// @brief 同上，针对裸字节区间。
std::vector<ca::u8> gzip_compress(const ca::u8* data, ca::usize size,
                                  int level = kGzipDefaultLevel);

}   // namespace ca::zip
