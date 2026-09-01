#include "libca/zip/input_stream.hpp"

#include <cstring>
#include <stdexcept>

#include "libca/zip/detail/inflate_raw.hpp"
#include "libca/zip/entry.hpp"
#include <zlib.h>

namespace ca::zip {

namespace {

constexpr ca::u32 kLocSig            = 0x04034b50;
constexpr ca::u32 kCenSig            = 0x02014b50;
constexpr ca::u32 kEndSig            = 0x06054b50;
constexpr ca::u32 kDataDescriptorSig = 0x08074b50;

ca::u16 read_u16(const ca::u8* p)
{
    return static_cast<ca::u16>(p[0] | (p[1] << 8));
}

ca::u32 read_u32(const ca::u8* p)
{
    return static_cast<ca::u32>(p[0] | (p[1] << 8) | (p[2] << 16) | (p[3] << 24));
}

}   // namespace

struct ZipInputStream::Impl
{
    std::vector<ca::u8> data;
    size_t              cursor = 0;

    // 当前条目状态
    std::string entry_name;
    ca::u16     entry_method            = 0;
    ca::u32     entry_crc32             = 0;
    ca::u32     entry_compressed_size   = 0;
    ca::u32     entry_uncompressed_size = 0;
    size_t      entry_data_start        = 0;
    size_t      entry_data_end          = 0;   // 压缩数据排他终点（含 DD 判定后）
    size_t      entry_data_offset       = 0;   // 已消费的压缩数据字节数
    bool        entry_open              = false;

    // Deflate 条目的 inflate 状态
    void* zstream      = nullptr;
    bool  zstream_init = false;
    bool  zstream_done = false;

    ~Impl()
    {
        if (zstream_init)
            ::inflateEnd(static_cast<z_stream*>(zstream));
    }

    // 自 pos 起向后扫描下一个 PK 头签名（LOC/CEN/EOCD）。
    size_t scan_next_header(size_t pos) const
    {
        for (size_t i = pos; i + 4 <= data.size(); ++i) {
            const ca::u32 sig = read_u32(&data[i]);
            if (sig == kLocSig || sig == kCenSig || sig == kEndSig)
                return i;
        }
        return data.size();
    }

    void release_zstream()
    {
        if (!zstream_init)
            return;
        ::inflateEnd(static_cast<z_stream*>(zstream));
        delete static_cast<z_stream*>(zstream);
        zstream      = nullptr;
        zstream_init = false;
    }
};

ZipInputStream::ZipInputStream(std::vector<ca::u8> data)
    : impl_(std::make_unique<Impl>())
{
    impl_->data = std::move(data);
}

ZipInputStream::~ZipInputStream() = default;

std::unique_ptr<ZipEntry> ZipInputStream::get_next_entry()
{
    if (impl_->entry_open)
        close_entry();

    auto&   data = impl_->data;
    size_t& pos  = impl_->cursor;

    if (pos + 30 > data.size())
        return nullptr;
    if (read_u32(&data[pos]) != kLocSig)
        return nullptr;

    const ca::u16 flags            = read_u16(&data[pos + 6]);
    impl_->entry_method            = read_u16(&data[pos + 8]);
    impl_->entry_crc32             = read_u32(&data[pos + 14]);
    impl_->entry_compressed_size   = read_u32(&data[pos + 18]);
    impl_->entry_uncompressed_size = read_u32(&data[pos + 22]);
    const ca::u16 nameLen          = read_u16(&data[pos + 26]);
    const ca::u16 extraLen         = read_u16(&data[pos + 28]);

    if (pos + 30 + nameLen + extraLen > data.size())
        return nullptr;

    impl_->entry_name.assign(reinterpret_cast<const char*>(&data[pos + 30]), nameLen);
    pos += 30 + nameLen + extraLen;
    impl_->entry_data_start  = pos;
    impl_->entry_data_offset = 0;
    impl_->entry_open        = true;
    impl_->zstream_done      = false;

    // bit 3 置位：尺寸字段不可信，靠扫描下一头部界定数据段。
    const bool hasDD = (flags & 0x0008) != 0;
    if (hasDD) {
        const size_t nextHdr = impl_->scan_next_header(pos);
        if (nextHdr == data.size()) {
            throw std::runtime_error("Data descriptor entry has no following ZIP header");
        }
        size_t dataEnd = nextHdr;

        // 优先按带签名的 16 字节 DD 形式切分。
        if (nextHdr >= 16 && nextHdr - 16 >= pos &&
            read_u32(&data[nextHdr - 16]) == kDataDescriptorSig) {
            dataEnd = nextHdr - 16;
        }
        else {
            // 回退到无签名的 12 字节旧式 DD；仅在 bit 3 合法，
            // 不适用于 LOC 尺寸本已权威的普通零长条目。
            if (nextHdr >= 12 && nextHdr - 12 >= pos) {
                dataEnd = nextHdr - 12;
            }
            else {
                throw std::runtime_error("Truncated data descriptor before next ZIP header");
            }
        }

        impl_->entry_compressed_size = static_cast<ca::u32>(dataEnd - pos);
    }
    if (static_cast<size_t>(pos) + impl_->entry_compressed_size > data.size()) {
        throw std::runtime_error("Entry compressed data exceeds file bounds");
    }
    impl_->entry_data_end = pos + impl_->entry_compressed_size;

    if (impl_->entry_method == 8) {
        impl_->release_zstream();
        auto* zs  = new z_stream{};
        int   ret = ::inflateInit2(zs, -15);
        if (ret != Z_OK) {
            delete zs;
            throw std::runtime_error("inflateInit2 failed");
        }
        impl_->zstream      = zs;
        impl_->zstream_init = true;
    }

    return std::make_unique<ZipEntry>(impl_->entry_name,
                                      impl_->entry_compressed_size,
                                      impl_->entry_uncompressed_size,
                                      impl_->entry_method,
                                      impl_->entry_crc32,
                                      0);
}

int ZipInputStream::read(ca::u8* buffer, size_t size)
{
    if (!impl_->entry_open)
        return 0;
    if (size == 0)
        return 0;

    auto& data = impl_->data;

    if (impl_->entry_method == 0) {
        const size_t remaining =
            impl_->entry_data_end - (impl_->entry_data_start + impl_->entry_data_offset);
        const size_t toRead = std::min(size, remaining);
        if (toRead == 0)
            return 0;
        std::memcpy(buffer, &data[impl_->entry_data_start + impl_->entry_data_offset], toRead);
        impl_->entry_data_offset += toRead;
        return static_cast<int>(toRead);
    }

    if (impl_->entry_method == 8) {
        if (impl_->zstream_done)
            return 0;

        auto* zs      = static_cast<z_stream*>(impl_->zstream);
        zs->next_out  = buffer;
        zs->avail_out = static_cast<uInt>(size);

        while (zs->avail_out > 0) {
            if (zs->avail_in == 0) {
                const size_t remaining =
                    impl_->entry_data_end - (impl_->entry_data_start + impl_->entry_data_offset);
                if (remaining == 0)
                    break;
                const size_t toFeed = std::min(remaining, size_t(8192));
                zs->next_in =
                    const_cast<ca::u8*>(&data[impl_->entry_data_start + impl_->entry_data_offset]);
                zs->avail_in = static_cast<uInt>(toFeed);
                impl_->entry_data_offset += toFeed;
            }

            const int ret = ::inflate(zs, Z_NO_FLUSH);
            if (ret == Z_STREAM_END) {
                impl_->zstream_done = true;
                break;
            }
            if (ret != Z_OK)
                break;
        }

        return static_cast<int>(size - zs->avail_out);
    }

    return -1;
}

std::vector<ca::u8> ZipInputStream::read_all()
{
    if (!impl_->entry_open)
        return {};

    if (impl_->entry_method == 0) {
        const size_t remaining =
            impl_->entry_data_end - (impl_->entry_data_start + impl_->entry_data_offset);
        std::vector<ca::u8> result(remaining);
        const int           n = read(result.data(), remaining);
        if (n < 0)
            return {};
        result.resize(static_cast<size_t>(n));
        return result;
    }

    if (impl_->entry_method == 8) {
        const size_t compressedRemaining =
            impl_->entry_data_end - (impl_->entry_data_start + impl_->entry_data_offset);
        if (impl_->zstream_init && impl_->zstream_done && compressedRemaining == 0) {
            impl_->release_zstream();
            return {};
        }
        const ca::u8* compressedStart = &impl_->data[impl_->entry_data_start];
        const size_t  totalCompressed = impl_->entry_data_end - impl_->entry_data_start;
        return inflate_raw(compressedStart, totalCompressed, impl_->entry_uncompressed_size);
    }

    return {};
}

void ZipInputStream::close_entry()
{
    if (!impl_->entry_open)
        return;
    impl_->entry_open = false;
    impl_->release_zstream();

    // 游标对齐：DD 之后即下一头部。
    auto&        data      = impl_->data;
    size_t       afterData = impl_->entry_data_end;
    const size_t nextHdr   = impl_->scan_next_header(afterData);
    if (nextHdr > afterData && nextHdr <= data.size()) {
        impl_->cursor = nextHdr;
    }
    else {
        impl_->cursor = afterData;
    }
}

void ZipInputStream::close()
{
    close_entry();
    impl_->cursor = 0;
    impl_->data.clear();
}

}   // namespace ca::zip
