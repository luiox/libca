#pragma once

#include <memory>
#include <vector>

#include "libca/core/datatype.hpp"
#include "libca/core/result.hpp"
#include "libca/zip/zip_error.hpp"

namespace ca::zip {

/// @brief 默认压缩级别（zlib 语义，-1 等价 Z_DEFAULT_COMPRESSION）。
inline constexpr int kGzipDefaultLevel = -1;

/// @brief RFC 1952 gzip 流式压缩写出器（内存汇聚）。
///
/// 构造即写入 10 字节 gzip 头（MTIME = 0，保证相同输入产出确定字节序列）；
/// write() 增量压缩，finish() 结束 deflate 流并追加 CRC32 与 ISIZE 尾部。
/// 完成后经 output()/take() 取得完整 gzip 字节。write/finish 的常规失败
/// （已 finish 再写、zlib 内部错误）经 Result 返回；压缩级别非法属于构造
/// 参数错误，构造重载抛 std::runtime_error。
class GzipWriter {
public:
    /// @brief 以默认压缩级别创建。
    GzipWriter();

    /// @brief 指定压缩级别（-1 默认 / 0-9）。
    /// @note 构造路径保留异常：级别非法抛 std::runtime_error。
    explicit GzipWriter(int level);

    /// @brief 释放 zlib 资源；不自动收尾。
    /// @note 未 finish 即析构时内部缓冲是不完整的 gzip 数据（缺 deflate 结束块与
    ///       CRC32/ISIZE 尾部），需要完整输出必须显式调用 finish()。
    ~GzipWriter();

    GzipWriter(const GzipWriter&)            = delete;
    GzipWriter& operator=(const GzipWriter&) = delete;

    /// @brief 压缩并暂存一段原始数据。
    /// @note finish 之后调用返回 Err(INVALID_STATE)；zlib 流错误返回
    ///       Err(ZLIB_ERROR)。
    Result<void, ZipErrorInfo> write(const ca::u8* data, ca::usize size);

    /// @brief 同上。
    Result<void, ZipErrorInfo> write(const std::vector<ca::u8>& data);

    /// @brief 收尾：结束 deflate 流并写 CRC32 与 ISIZE 尾部；重复调用无害。
    Result<void, ZipErrorInfo> finish();

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
/// @param level 压缩级别（-1 默认 / 0-9）；非法值返回 Err(INVALID_ARGUMENT)。
/// @return 完整 gzip 字节序列（头 + deflate 数据 + CRC32/ISIZE 尾）。
Result<std::vector<ca::u8>, ZipErrorInfo> gzip_compress(const std::vector<ca::u8>& data, int level = kGzipDefaultLevel);

/// @brief 同上，针对裸字节区间。
Result<std::vector<ca::u8>, ZipErrorInfo> gzip_compress(const ca::u8* data, ca::usize size,
                                                        int level = kGzipDefaultLevel);

}   // namespace ca::zip
