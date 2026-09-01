#pragma once

#include <string>
#include <vector>

#include "libca/core/datatype.hpp"

namespace ca::zip {

/// @brief ZIP 条目元数据（对应 java.util.zip.ZipEntry 的信息面）。
class ZipEntry
{
public:
    ZipEntry() = default;
    ZipEntry(std::string name, ca::u32 compressed_size, ca::u32 uncompressed_size,
             ca::u16 compression_method, ca::u32 crc32, ca::u32 relative_offset,
             std::vector<ca::u8> extra_field = {})
        : name_(std::move(name))
        , compressed_size_(compressed_size)
        , uncompressed_size_(uncompressed_size)
        , compression_method_(compression_method)
        , crc32_(crc32)
        , relative_offset_(relative_offset)
        , extra_field_(std::move(extra_field))
    {}

    const std::string&         name() const { return name_; }
    ca::u32                    compressed_size() const { return compressed_size_; }
    ca::u32                    uncompressed_size() const { return uncompressed_size_; }
    ca::u16                    compression_method() const { return compression_method_; }
    ca::u32                    crc32() const { return crc32_; }
    ca::u32                    relative_offset() const { return relative_offset_; }
    const std::vector<ca::u8>& extra_field() const { return extra_field_; }

    /// @brief 是否 Deflate（method 8）。
    bool is_deflated() const { return compression_method_ == 8; }

    /// @brief 是否 Stored（method 0，无压缩）。
    bool is_stored() const { return compression_method_ == 0; }

    void set_name(const std::string& name) { name_ = name; }
    void set_compressed_size(ca::u32 v) { compressed_size_ = v; }
    void set_uncompressed_size(ca::u32 v) { uncompressed_size_ = v; }
    void set_compression_method(ca::u16 v) { compression_method_ = v; }
    void set_crc32(ca::u32 v) { crc32_ = v; }
    void set_relative_offset(ca::u32 v) { relative_offset_ = v; }
    void set_extra_field(std::vector<ca::u8> v) { extra_field_ = std::move(v); }

private:
    std::string         name_;
    ca::u32             compressed_size_    = 0;
    ca::u32             uncompressed_size_  = 0;
    ca::u16             compression_method_ = 0;
    ca::u32             crc32_              = 0;
    ca::u32             relative_offset_    = 0;
    std::vector<ca::u8> extra_field_;
};

}   // namespace ca::zip
