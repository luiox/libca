#pragma once

#include <memory>
#include <string>
#include <vector>

#include "libca/core/datatype.hpp"
#include "libca/core/result.hpp"
#include "libca/zip/zip_error.hpp"

namespace ca::zip {

class ZipEntry;

/// @brief 流式写出 ZIP（对应 java.util.zip.ZipOutputStream 语义）。
///
/// 每条目以 data descriptor 收尾（bit 3 置位），CEN/EOCD 在 close 时统一回写。
/// 常规失败（磁盘写失败、状态机误用、非法压缩级别等）经 Result 返回；
/// 异常仅保留在打开型构造重载（失败抛 std::runtime_error）。
class ZipOutputStream {
public:
    ZipOutputStream();

    /// @brief 直接写磁盘文件。
    /// @note 构造路径保留异常：失败抛 std::runtime_error；需要 Result 语义请
    ///       先默认构造再调 open(path)。
    explicit ZipOutputStream(const std::string& path);

    /// @brief 若仍处于打开状态，先完成 CEN/EOCD 回写再关闭（错误静默忽略）。
    ~ZipOutputStream();

    /// @brief 打开磁盘文件准备写入；已处于打开状态返回 Err(INVALID_STATE)。
    Result<void, ZipErrorInfo> open(const std::string& path);

    bool is_open() const;

    /// @brief 完成 CEN/EOCD 回写并关闭；未打开时为无害空操作返回 Ok。
    Result<void, ZipErrorInfo> close();

    /// @brief 开始写入新条目（LOC 头立即落盘）。
    /// @note 流未 open 时返回 Err(INVALID_STATE)。
    Result<void, ZipErrorInfo> put_next_entry(const ZipEntry& entry);

    /// @brief 追加当前条目的原始字节；自动累计 CRC 与尺寸。
    /// @note 无当前条目返回 Err(INVALID_STATE)。
    Result<void, ZipErrorInfo> write(const ca::u8* data, size_t size);

    /// @brief 同上。
    Result<void, ZipErrorInfo> write(const std::vector<ca::u8>& data);

    /// @brief 结束当前条目：收尾 deflate、落 DD、缓存 CEN 记录。
    Result<void, ZipErrorInfo> close_entry();

    /// @brief 设置 Deflate 压缩级别（-1 默认 / 0-9）；非法值返回 Err(INVALID_ARGUMENT)。
    Result<void, ZipErrorInfo> set_level(int level);

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

}   // namespace ca::zip
