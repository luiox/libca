#include "libca/zip/gzip_writer.hpp"

#include <stdexcept>
#include <string>

#include <zlib.h>

#include "libca/zip/checksum.hpp"

namespace ca::zip {

namespace {

constexpr ca::u8 kMagic1     = 0x1F;
constexpr ca::u8 kMagic2     = 0x8B;
constexpr ca::u8 kDeflateCm  = 8;
constexpr ca::u8 kOsUnknown  = 0xFF;

void write_u32_le(ca::u8* p, ca::u32 v)
{
    p[0] = static_cast<ca::u8>(v);
    p[1] = static_cast<ca::u8>(v >> 8);
    p[2] = static_cast<ca::u8>(v >> 16);
    p[3] = static_cast<ca::u8>(v >> 24);
}

void append_u32_le(std::vector<ca::u8>& out, ca::u32 v)
{
    ca::u8 buf[4];
    write_u32_le(buf, v);
    out.insert(out.end(), buf, buf + 4);
}

}   // anonymous namespace

struct GzipWriter::Impl {
    int                 level = Z_DEFAULT_COMPRESSION;
    z_stream            zs {};
    bool                zs_init  = false;
    bool                finished = false;
    std::vector<ca::u8> out;
    Crc32               crc;
    ca::u64             total_uncompressed = 0;

    ~Impl()
    {
        if (zs_init) {
            ::deflateEnd(&zs);
        }
    }
};

GzipWriter::GzipWriter()
    : impl_(std::make_unique<Impl>())
{
    init_impl(kGzipDefaultLevel);
}

GzipWriter::GzipWriter(int level)
    : impl_(std::make_unique<Impl>())
{
    init_impl(level);
}

void GzipWriter::init_impl(int level)
{
    if (level < -1 || level > 9) {
        throw std::runtime_error("Invalid compression level: " + std::to_string(level));
    }
    impl_->level = level;

    // RFC 1952 固定头：magic + CM + FLG=0 + MTIME=0 + XFL + OS=unknown。
    // MTIME 取 0 使输出只由输入与级别决定，便于测试与互操作比对。
    ca::u8 header[10] = {};
    header[0] = kMagic1;
    header[1] = kMagic2;
    header[2] = kDeflateCm;
    header[3] = 0;
    // XFL 语义（gzip 实现）：级别 9 置 2，级别 1 置 4，其余 0。
    if (level == 9) {
        header[8] = 2;
    } else if (level == 1) {
        header[8] = 4;
    }
    header[9] = kOsUnknown;
    impl_->out.insert(impl_->out.end(), header, header + sizeof(header));

    if (::deflateInit2(&impl_->zs, level, Z_DEFLATED, -MAX_WBITS, 8, Z_DEFAULT_STRATEGY) !=
        Z_OK) {
        throw std::runtime_error("zlib deflateInit2 failed");
    }
    impl_->zs_init = true;
}

GzipWriter::~GzipWriter() = default;

void GzipWriter::write(const ca::u8* data, ca::usize size)
{
    if (impl_->finished) {
        throw std::runtime_error("GzipWriter already finished");
    }
    if (size == 0) {
        return;
    }

    impl_->crc.update(data, size);
    impl_->total_uncompressed += size;

    auto* zs = &impl_->zs;

    // avail_in 为 uInt（32 位），超大输入分批喂入。
    constexpr ca::usize kMaxFeed = static_cast<ca::usize>(0x7FFFFFFF);
    while (size > 0) {
        const ca::usize chunk = size > kMaxFeed ? kMaxFeed : size;
        zs->next_in          = const_cast<Bytef*>(reinterpret_cast<const Bytef*>(data));
        zs->avail_in         = static_cast<uInt>(chunk);

        ca::u8 buffer[8192];
        do {
            zs->next_out  = buffer;
            zs->avail_out = sizeof(buffer);
            const int ret = ::deflate(zs, Z_NO_FLUSH);
            if (ret == Z_STREAM_ERROR) {
                throw std::runtime_error("deflate stream error");
            }
            impl_->out.insert(impl_->out.end(), buffer, buffer + (sizeof(buffer) - zs->avail_out));
        } while (zs->avail_out == 0);

        data += chunk;
        size -= chunk;
    }
}

void GzipWriter::write(const std::vector<ca::u8>& data)
{
    write(data.data(), data.size());
}

void GzipWriter::finish()
{
    if (impl_->finished) {
        return;
    }

    auto* zs = &impl_->zs;
    zs->next_in  = nullptr;
    zs->avail_in = 0;

    ca::u8 buffer[8192];
    int    ret = Z_OK;
    do {
        zs->next_out  = buffer;
        zs->avail_out = sizeof(buffer);
        ret           = ::deflate(zs, Z_FINISH);
        if (ret == Z_STREAM_ERROR) {
            throw std::runtime_error("deflate stream error on finish");
        }
        impl_->out.insert(impl_->out.end(), buffer, buffer + (sizeof(buffer) - zs->avail_out));
    } while (ret != Z_STREAM_END);

    ::deflateEnd(zs);
    impl_->zs_init = false;

    // 尾部：CRC32（未压缩数据）+ ISIZE（未压缩长度 mod 2^32）。
    append_u32_le(impl_->out, impl_->crc.value());
    append_u32_le(impl_->out, static_cast<ca::u32>(impl_->total_uncompressed & 0xFFFFFFFFu));

    impl_->finished = true;
}

bool GzipWriter::finished() const
{
    return impl_->finished;
}

const std::vector<ca::u8>& GzipWriter::output() const
{
    return impl_->out;
}

std::vector<ca::u8> GzipWriter::take()
{
    std::vector<ca::u8> result = std::move(impl_->out);
    impl_->out.clear();
    return result;
}

std::vector<ca::u8> gzip_compress(const std::vector<ca::u8>& data, int level)
{
    return gzip_compress(data.data(), data.size(), level);
}

std::vector<ca::u8> gzip_compress(const ca::u8* data, ca::usize size, int level)
{
    GzipWriter writer(level);
    writer.write(data, size);
    writer.finish();
    return writer.take();
}

}   // namespace ca::zip
