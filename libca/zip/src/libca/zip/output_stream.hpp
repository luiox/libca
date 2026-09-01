#pragma once

#include <memory>
#include <string>
#include <vector>

#include "libca/core/datatype.hpp"

namespace ca::zip {

class ZipEntry;

/// @brief 流式写出 ZIP（对应 java.util.zip.ZipOutputStream 语义）。
///
/// 每条目以 data descriptor 收尾（bit 3 置位），CEN/EOCD 在 close 时统一回写。
/// 析构时若仍打开则尝试收尾（不抛异常）。
class ZipOutputStream
{
public:
    ZipOutputStream();

    /// @brief 直接写磁盘文件。
    explicit ZipOutputStream(const std::string& path);

    /// @brief 若仍处于打开状态，先完成 CEN/EOCD 回写再关闭。
    ~ZipOutputStream();

    void open(const std::string& path);
    bool is_open() const;
    void close();

    /// @brief 开始写入新条目（LOC 头立即落盘）。
    void put_next_entry(const ZipEntry& entry);

    /// @brief 追加当前条目的原始字节；自动累计 CRC 与尺寸。
    void write(const ca::u8* data, size_t size);
    void write(const std::vector<ca::u8>& data);

    /// @brief 结束当前条目：收尾 deflate、落 DD、缓存 CEN 记录。
    void close_entry();

    /// @brief 设置 Deflate 压缩级别（-1 默认 / 0-9）；非法值抛异常。
    void set_level(int level);

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

}   // namespace ca::zip
