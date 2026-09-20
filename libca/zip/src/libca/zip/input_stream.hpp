#pragma once

#include <memory>
#include <string>
#include <vector>

#include "libca/core/datatype.hpp"
#include "libca/core/result.hpp"
#include "libca/zip/zip_error.hpp"

namespace ca::zip {

class ZipEntry;

/// @brief 顺序流式读取 ZIP（对应 java.util.zip.ZipInputStream 的 LOC 直读语义）。
///
/// 从数据流头部逐条消费本地文件头；支持 data descriptor 条目。
/// 结构损坏（data descriptor 越界、压缩数据越界等）经 Result 返回，
/// 不抛异常。
class ZipInputStream {
public:
    /// @brief 接管整段 ZIP 数据。
    explicit ZipInputStream(std::vector<ca::u8> data);
    ~ZipInputStream();

    /// @brief 定位下一条目并返回其元数据。
    /// @return 流正常耗尽返回 Ok(空指针)；结构损坏返回 Err(INVALID_FORMAT)。
    Result<std::unique_ptr<ZipEntry>, ZipErrorInfo> get_next_entry();

    /// @brief 读取当前条目的解压数据，返回实际字节数。
    /// @note 条目未打开或已读完返回 Ok(0)；不支持的压缩方法返回
    ///       Err(UNSUPPORTED)；deflate 数据损坏返回 Err(INVALID_FORMAT)。
    Result<int, ZipErrorInfo> read(ca::u8* buffer, size_t size);

    /// @brief 读完并返回当前条目全部内容。
    Result<std::vector<ca::u8>, ZipErrorInfo> read_all();

    /// @brief 结束当前条目，跳过 data descriptor 对齐到下一头部。
    void close_entry();

    /// @brief 关闭流并清空缓冲。
    void close();

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

}   // namespace ca::zip
