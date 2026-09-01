#pragma once

// 测试专用内联 ZIP 构造工具：不依赖任何外部样本文件，
// 归档字节在测试内按 LOC/CEN/EOCD 布局逐段拼出。

#include <gtest/gtest.h>

#include <cstdint>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

#include <zlib.h>

namespace zip_test {

inline void append_u16_le(std::vector<uint8_t>& out, uint16_t v)
{
    out.push_back(static_cast<uint8_t>(v & 0xFF));
    out.push_back(static_cast<uint8_t>((v >> 8) & 0xFF));
}

inline void append_u32_le(std::vector<uint8_t>& out, uint32_t v)
{
    out.push_back(static_cast<uint8_t>(v & 0xFF));
    out.push_back(static_cast<uint8_t>((v >> 8) & 0xFF));
    out.push_back(static_cast<uint8_t>((v >> 16) & 0xFF));
    out.push_back(static_cast<uint8_t>((v >> 24) & 0xFF));
}

inline void append_u64_le(std::vector<uint8_t>& out, uint64_t v)
{
    append_u32_le(out, static_cast<uint32_t>(v & 0xFFFFFFFFu));
    append_u32_le(out, static_cast<uint32_t>(v >> 32));
}

inline uint32_t crc32_of(const uint8_t* data, size_t len)
{
    return static_cast<uint32_t>(::crc32(0, data, static_cast<uInt>(len)));
}

template<typename T>
inline uint32_t crc32_of(const T& container)
{
    return crc32_of(reinterpret_cast<const uint8_t*>(container.data()), container.size());
}

// 单条 stored 条目的最小合法 ZIP。
inline std::vector<uint8_t> build_minimal_stored_zip(const std::string& name    = "a.txt",
                                                     const std::string& content = "abc")
{
    const std::vector<uint8_t> data(content.begin(), content.end());
    const uint32_t             crc  = crc32_of(data);
    const uint32_t             size = static_cast<uint32_t>(data.size());

    std::vector<uint8_t> out;

    append_u32_le(out, 0x04034b50u);
    append_u16_le(out, 20);
    append_u16_le(out, 0);
    append_u16_le(out, 0);
    append_u16_le(out, 0);
    append_u16_le(out, 0);
    append_u32_le(out, crc);
    append_u32_le(out, size);
    append_u32_le(out, size);
    append_u16_le(out, static_cast<uint16_t>(name.size()));
    append_u16_le(out, 0);
    out.insert(out.end(), name.begin(), name.end());
    out.insert(out.end(), data.begin(), data.end());

    const uint32_t cenOffset = static_cast<uint32_t>(out.size());

    append_u32_le(out, 0x02014b50u);
    append_u16_le(out, 20);
    append_u16_le(out, 20);
    append_u16_le(out, 0);
    append_u16_le(out, 0);
    append_u16_le(out, 0);
    append_u16_le(out, 0);
    append_u32_le(out, crc);
    append_u32_le(out, size);
    append_u32_le(out, size);
    append_u16_le(out, static_cast<uint16_t>(name.size()));
    append_u16_le(out, 0);
    append_u16_le(out, 0);
    append_u16_le(out, 0);
    append_u16_le(out, 0);
    append_u32_le(out, 0);
    append_u32_le(out, 0);
    out.insert(out.end(), name.begin(), name.end());

    const uint32_t cenSize = static_cast<uint32_t>(out.size()) - cenOffset;

    append_u32_le(out, 0x06054b50u);
    append_u16_le(out, 0);
    append_u16_le(out, 0);
    append_u16_le(out, 1);
    append_u16_le(out, 1);
    append_u32_le(out, cenSize);
    append_u32_le(out, cenOffset);
    append_u16_le(out, 0);

    return out;
}

// 带 "PREFIX" 前缀的自提取式 ZIP64：CEN 偏移相对 ZIP 段自身。
inline std::vector<uint8_t> build_prefixed_zip64_zip()
{
    const std::string          name = "zip64.txt";
    const std::vector<uint8_t> data = {'z', 'i', 'p', '6', '4'};
    const uint32_t             crc  = crc32_of(data);
    const uint32_t             size = static_cast<uint32_t>(data.size());

    std::vector<uint8_t> out        = {'P', 'R', 'E', 'F', 'I', 'X'};
    const uint64_t       baseOffset = out.size();

    const uint64_t locOffset = out.size();
    append_u32_le(out, 0x04034b50u);
    append_u16_le(out, 45);
    append_u16_le(out, 0);
    append_u16_le(out, 0);
    append_u16_le(out, 0);
    append_u16_le(out, 0);
    append_u32_le(out, crc);
    append_u32_le(out, size);
    append_u32_le(out, size);
    append_u16_le(out, static_cast<uint16_t>(name.size()));
    append_u16_le(out, 0);
    out.insert(out.end(), name.begin(), name.end());
    out.insert(out.end(), data.begin(), data.end());

    const uint64_t cenStart = out.size();
    append_u32_le(out, 0x02014b50u);
    append_u16_le(out, 45);
    append_u16_le(out, 45);
    append_u16_le(out, 0);
    append_u16_le(out, 0);
    append_u16_le(out, 0);
    append_u16_le(out, 0);
    append_u32_le(out, crc);
    append_u32_le(out, size);
    append_u32_le(out, size);
    append_u16_le(out, static_cast<uint16_t>(name.size()));
    append_u16_le(out, 0);
    append_u16_le(out, 0);
    append_u16_le(out, 0);
    append_u16_le(out, 0);
    append_u32_le(out, 0);
    append_u32_le(out, static_cast<uint32_t>(locOffset - baseOffset));
    out.insert(out.end(), name.begin(), name.end());

    const uint64_t cenSize        = out.size() - cenStart;
    const uint64_t zip64EndOffset = out.size();

    append_u32_le(out, 0x06064b50u);
    append_u64_le(out, 44);
    append_u16_le(out, 45);
    append_u16_le(out, 45);
    append_u32_le(out, 0);
    append_u32_le(out, 0);
    append_u64_le(out, 1);
    append_u64_le(out, 1);
    append_u64_le(out, cenSize);
    append_u64_le(out, cenStart - baseOffset);

    append_u32_le(out, 0x07064b50u);
    append_u32_le(out, 0);
    append_u64_le(out, zip64EndOffset - baseOffset);
    append_u32_le(out, 1);

    append_u32_le(out, 0x06054b50u);
    append_u16_le(out, 0);
    append_u16_le(out, 0);
    append_u16_le(out, 0xFFFF);
    append_u16_le(out, 0xFFFF);
    append_u32_le(out, 0xFFFFFFFFu);
    append_u32_le(out, 0xFFFFFFFFu);
    append_u16_le(out, 0);

    return out;
}

// CEN 中 CRC 与 LOC 数据不一致的归档：读取时应报 CRC 校验失败。
inline std::vector<uint8_t> build_crc_mismatch_zip()
{
    const std::string          name       = "a.txt";
    const std::vector<uint8_t> data       = {'a', 'b', 'c'};
    const uint32_t             correctCrc = crc32_of(data);
    const uint32_t             wrongCrc   = 0xDEADBEEF;
    const uint32_t             size       = static_cast<uint32_t>(data.size());

    std::vector<uint8_t> out;

    append_u32_le(out, 0x04034b50u);
    append_u16_le(out, 20);
    append_u16_le(out, 0);
    append_u16_le(out, 0);
    append_u16_le(out, 0);
    append_u16_le(out, 0);
    append_u32_le(out, correctCrc);
    append_u32_le(out, size);
    append_u32_le(out, size);
    append_u16_le(out, static_cast<uint16_t>(name.size()));
    append_u16_le(out, 0);
    out.insert(out.end(), name.begin(), name.end());
    out.insert(out.end(), data.begin(), data.end());

    const uint32_t cenOffset = static_cast<uint32_t>(out.size());

    append_u32_le(out, 0x02014b50u);
    append_u16_le(out, 20);
    append_u16_le(out, 20);
    append_u16_le(out, 0);
    append_u16_le(out, 0);
    append_u16_le(out, 0);
    append_u16_le(out, 0);
    append_u32_le(out, wrongCrc);
    append_u32_le(out, size);
    append_u32_le(out, size);
    append_u16_le(out, static_cast<uint16_t>(name.size()));
    append_u16_le(out, 0);
    append_u16_le(out, 0);
    append_u16_le(out, 0);
    append_u16_le(out, 0);
    append_u32_le(out, 0);
    append_u32_le(out, 0);
    out.insert(out.end(), name.begin(), name.end());

    const uint32_t cenSize = static_cast<uint32_t>(out.size()) - cenOffset;

    append_u32_le(out, 0x06054b50u);
    append_u16_le(out, 0);
    append_u16_le(out, 0);
    append_u16_le(out, 1);
    append_u16_le(out, 1);
    append_u32_le(out, cenSize);
    append_u32_le(out, cenOffset);
    append_u16_le(out, 0);

    return out;
}

// 单条 stored 条目 + 无签名 12 字节旧式 data descriptor。
inline std::vector<uint8_t> build_stored_zip_with_unsigned_data_descriptor()
{
    constexpr uint16_t         kDdFlags = 0x0008u;
    std::vector<uint8_t>       out;
    const std::string          name = "a.txt";
    const std::vector<uint8_t> data = {'a', 'b', 'c'};

    append_u32_le(out, 0x04034b50u);
    append_u16_le(out, 20);
    append_u16_le(out, kDdFlags);
    append_u16_le(out, 0);
    append_u16_le(out, 0);
    append_u16_le(out, 0);
    append_u32_le(out, 0);
    append_u32_le(out, 0);
    append_u32_le(out, 0);
    append_u16_le(out, static_cast<uint16_t>(name.size()));
    append_u16_le(out, 0);
    out.insert(out.end(), name.begin(), name.end());
    out.insert(out.end(), data.begin(), data.end());

    // 无签名 12 字节 data descriptor
    append_u32_le(out, 0);
    append_u32_le(out, static_cast<uint32_t>(data.size()));
    append_u32_le(out, static_cast<uint32_t>(data.size()));

    const uint32_t cdOffset = static_cast<uint32_t>(out.size());
    append_u32_le(out, 0x02014b50u);
    append_u16_le(out, 20);
    append_u16_le(out, 20);
    append_u16_le(out, kDdFlags);
    append_u16_le(out, 0);
    append_u16_le(out, 0);
    append_u16_le(out, 0);
    append_u32_le(out, 0);
    append_u32_le(out, static_cast<uint32_t>(data.size()));
    append_u32_le(out, static_cast<uint32_t>(data.size()));
    append_u16_le(out, static_cast<uint16_t>(name.size()));
    append_u16_le(out, 0);
    append_u16_le(out, 0);
    append_u16_le(out, 0);
    append_u16_le(out, 0);
    append_u32_le(out, 0);
    append_u32_le(out, 0);
    out.insert(out.end(), name.begin(), name.end());

    const uint32_t cdSize = static_cast<uint32_t>(out.size()) - cdOffset;
    append_u32_le(out, 0x06054b50u);
    append_u16_le(out, 0);
    append_u16_le(out, 0);
    append_u16_le(out, 1);
    append_u16_le(out, 1);
    append_u32_le(out, cdSize);
    append_u32_le(out, cdOffset);
    append_u16_le(out, 0);
    return out;
}

// 首条目零长、次条目有数据的双 LOC 流：验证空条目不吞后续头部。
inline std::vector<uint8_t> build_two_local_headers_with_empty_first_entry()
{
    std::vector<uint8_t>       out;
    const std::string          emptyName = "empty.txt";
    const std::string          dataName  = "b.txt";
    const std::vector<uint8_t> data      = {'b', 'c', 'd'};

    append_u32_le(out, 0x04034b50u);
    append_u16_le(out, 20);
    append_u16_le(out, 0);
    append_u16_le(out, 0);
    append_u16_le(out, 0);
    append_u16_le(out, 0);
    append_u32_le(out, 0);
    append_u32_le(out, 0);
    append_u32_le(out, 0);
    append_u16_le(out, static_cast<uint16_t>(emptyName.size()));
    append_u16_le(out, 0);
    out.insert(out.end(), emptyName.begin(), emptyName.end());

    append_u32_le(out, 0x04034b50u);
    append_u16_le(out, 20);
    append_u16_le(out, 0);
    append_u16_le(out, 0);
    append_u16_le(out, 0);
    append_u16_le(out, 0);
    append_u32_le(out, 0);
    append_u32_le(out, static_cast<uint32_t>(data.size()));
    append_u32_le(out, static_cast<uint32_t>(data.size()));
    append_u16_le(out, static_cast<uint16_t>(dataName.size()));
    append_u16_le(out, 0);
    out.insert(out.end(), dataName.begin(), dataName.end());
    out.insert(out.end(), data.begin(), data.end());
    return out;
}

// 截断无签名 DD 的坏档：下一头部存在但 DD 本身缺尾 4 字节。
inline std::vector<uint8_t> build_truncated_unsigned_data_descriptor_zip()
{
    auto out = build_stored_zip_with_unsigned_data_descriptor();
    out.erase(out.begin() + 38, out.begin() + 42);
    return out;
}

// 测试输出路径：系统临时目录下 libca_zip_test/ 子目录（自动创建）。
inline std::filesystem::path temp_path(const std::string& filename)
{
    namespace fs = std::filesystem;
    auto dir     = fs::temp_directory_path() / "libca_zip_test";
    fs::create_directories(dir);
    return dir / filename;
}

inline std::vector<uint8_t> read_file_bytes(const std::filesystem::path& path)
{
    std::ifstream in(path, std::ios::binary);
    if (!in) {
        ADD_FAILURE() << "cannot open file: " << path.string();
        return {};
    }
    return std::vector<uint8_t>((std::istreambuf_iterator<char>(in)),
                                std::istreambuf_iterator<char>());
}

}   // namespace zip_test
