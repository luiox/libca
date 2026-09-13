#include "libca/zip/gzip_reader.hpp"

#include <cstring>
#include <stdexcept>
#include <string>

#include <zlib.h>

#include "libca/zip/checksum.hpp"

namespace ca::zip {

namespace {

constexpr ca::u8 kGzipMagic1 = 0x1F;
constexpr ca::u8 kGzipMagic2 = 0x8B;
constexpr ca::u8 kDeflateCm  = 8;

// RFC 1952 FLG 位定义；bit 5-7 为保留位，必须为 0。
constexpr ca::u8 kFlagFtext    = 0x01;
constexpr ca::u8 kFlagFhcrc    = 0x02;
constexpr ca::u8 kFlagFextra   = 0x04;
constexpr ca::u8 kFlagFname    = 0x08;
constexpr ca::u8 kFlagFcomment = 0x10;
constexpr ca::u8 kFlagReserved = 0xE0;

constexpr ca::usize kTrailerSize   = 8;   // CRC32 + ISIZE
constexpr ca::usize kFixedHeadSize = 10;

ca::u16 read_u16(const ca::u8* p)
{
    return static_cast<ca::u16>(p[0] | (p[1] << 8));
}

ca::u32 read_u32(const ca::u8* p)
{
    return static_cast<ca::u32>(p[0] | (p[1] << 8) | (p[2] << 16) | (p[3] << 24));
}

}   // anonymous namespace

struct GzipReader::Impl {
    // 成员生命周期状态机：kHeader 解析头 → kDeflate 解压体 → kTrailer 校验尾
    // → 还有剩余输入则回到 kHeader（多成员拼接），否则 kDone。
    enum class State {
        kHeader,
        kDeflate,
        kTrailer,
        kDone,
    };

    std::vector<ca::u8> data;
    ca::usize           cursor = 0;
    State               state  = State::kHeader;

    z_stream zs {};
    bool     zs_init = false;

    Crc32   crc;
    ca::u64 member_uncompressed_size = 0;   // 当前成员累计解压字节数（ISIZE 校验用）

    ~Impl()
    {
        if (zs_init) {
            ::inflateEnd(&zs);
        }
    }

    void release_zstream()
    {
        if (!zs_init) {
            return;
        }
        ::inflateEnd(&zs);
        zs_init = false;
    }

    // 解析从 cursor 起的成员头，返回头总长（含可选字段与 FHCRC）。
    // 输入不足以容纳头或任一变长字段按流截断处理。
    ca::usize parse_member_header()
    {
        const ca::u8*  p     = data.data() + cursor;
        const ca::usize avail = data.size() - cursor;

        if (avail < kFixedHeadSize) {
            throw std::runtime_error("truncated gzip header");
        }
        if (p[0] != kGzipMagic1 || p[1] != kGzipMagic2) {
            throw std::runtime_error("bad gzip magic");
        }
        if (p[2] != kDeflateCm) {
            throw std::runtime_error("unsupported gzip compression method: " +
                                     std::to_string(p[2]));
        }
        const ca::u8 flg = p[3];
        if ((flg & kFlagReserved) != 0) {
            throw std::runtime_error("reserved gzip header flags set");
        }

        // MTIME/XFL/OS 不参与解压语义，直接跳过。
        ca::usize offset = kFixedHeadSize;

        if ((flg & kFlagFextra) != 0) {
            if (avail < offset + 2) {
                throw std::runtime_error("truncated gzip FEXTRA length");
            }
            const ca::u16 xlen = read_u16(p + offset);
            offset += 2;
            if (avail < offset + xlen) {
                throw std::runtime_error("truncated gzip FEXTRA data");
            }
            offset += xlen;
        }
        if ((flg & kFlagFname) != 0) {
            offset = skip_nul_terminated_field(p, avail, offset, "FNAME");
        }
        if ((flg & kFlagFcomment) != 0) {
            offset = skip_nul_terminated_field(p, avail, offset, "FCOMMENT");
        }

        if ((flg & kFlagFhcrc) != 0) {
            if (avail < offset + 2) {
                throw std::runtime_error("truncated gzip FHCRC");
            }
            // FHCRC 为头前缀（不含自身）CRC32 的低 16 位。
            const ca::u16 stored_crc16 = read_u16(p + offset);
            Crc32         header_crc;
            header_crc.update(p, offset);
            if (static_cast<ca::u16>(header_crc.value() & 0xFFFFu) != stored_crc16) {
                throw std::runtime_error("gzip header CRC16 mismatch");
            }
            offset += 2;
        }

        return offset;
    }

    // 跳过 NUL 结尾的变长字段；越界（缺终止符）按流截断处理。
    static ca::usize skip_nul_terminated_field(const ca::u8* p, ca::usize avail,
                                               ca::usize offset, const char* field)
    {
        while (offset < avail) {
            if (p[offset] == 0) {
                return offset + 1;
            }
            ++offset;
        }
        throw std::runtime_error(std::string("truncated gzip ") + field);
    }

    // cursor 处开始新成员：解析头并初始化 raw deflate inflate。
    void begin_member()
    {
        cursor += parse_member_header();

        release_zstream();
        std::memset(&zs, 0, sizeof(zs));
        if (::inflateInit2(&zs, -MAX_WBITS) != Z_OK) {
            throw std::runtime_error("zlib inflateInit2 failed");
        }
        zs_init                 = true;
        crc                     = Crc32();
        member_uncompressed_size = 0;
        state                   = State::kDeflate;
    }

    // 校验成员尾并推进状态：有剩余输入继续下一成员，否则完成。
    void finish_member()
    {
        if (data.size() - cursor < kTrailerSize) {
            throw std::runtime_error("truncated gzip trailer");
        }
        const ca::u32 stored_crc   = read_u32(data.data() + cursor);
        const ca::u32 stored_isize = read_u32(data.data() + cursor + 4);
        cursor += kTrailerSize;

        release_zstream();

        if (crc.value() != stored_crc) {
            throw std::runtime_error("gzip CRC32 mismatch");
        }
        if (static_cast<ca::u32>(member_uncompressed_size & 0xFFFFFFFFu) != stored_isize) {
            throw std::runtime_error("gzip ISIZE mismatch");
        }

        state = cursor < data.size() ? State::kHeader : State::kDone;
    }
};

GzipReader::GzipReader(std::vector<ca::u8> data)
    : impl_(std::make_unique<Impl>())
{
    impl_->data = std::move(data);
}

GzipReader::GzipReader(const ca::u8* data, ca::usize size)
    : GzipReader(size > 0 && data != nullptr ? std::vector<ca::u8>(data, data + size)
                                             : std::vector<ca::u8>())
{}

GzipReader::~GzipReader() = default;

int GzipReader::read(ca::u8* buffer, ca::usize size)
{
    ca::usize filled = 0;

    while (filled < size && impl_->state != Impl::State::kDone) {
        switch (impl_->state) {
        case Impl::State::kHeader: {
            impl_->begin_member();
            break;
        }
        case Impl::State::kDeflate: {
            if (impl_->cursor >= impl_->data.size()) {
                throw std::runtime_error("truncated gzip stream: unexpected end of deflate data");
            }

            auto*  zs       = &impl_->zs;
            size_t feed     = impl_->data.size() - impl_->cursor;
            constexpr size_t kMaxFeed = static_cast<size_t>(0x7FFFFFFF);
            if (feed > kMaxFeed) {
                feed = kMaxFeed;
            }
            zs->next_in  = const_cast<Bytef*>(reinterpret_cast<const Bytef*>(
                impl_->data.data() + impl_->cursor));
            zs->avail_in = static_cast<uInt>(feed);
            zs->next_out  = buffer + filled;
            zs->avail_out = static_cast<uInt>(size - filled);

            const int ret = ::inflate(zs, Z_NO_FLUSH);
            const size_t consumed = feed - zs->avail_in;
            const size_t produced = static_cast<size_t>(size - filled) - zs->avail_out;
            impl_->cursor += consumed;
            filled += produced;
            impl_->member_uncompressed_size += produced;
            impl_->crc.update(buffer + filled - produced, produced);

            if (ret == Z_STREAM_END) {
                impl_->state = Impl::State::kTrailer;
            } else if (ret != Z_OK) {
                throw std::runtime_error("zlib raw inflate failed (" + std::to_string(ret) + ")");
            }
            break;
        }
        case Impl::State::kTrailer: {
            impl_->finish_member();
            break;
        }
        case Impl::State::kDone: {
            break;
        }
        }
    }

    return static_cast<int>(filled);
}

std::vector<ca::u8> GzipReader::read_all()
{
    std::vector<ca::u8> out;
    ca::u8              buffer[16384];
    while (true) {
        const int n = read(buffer, sizeof(buffer));
        if (n <= 0) {
            break;
        }
        out.insert(out.end(), buffer, buffer + n);
    }
    return out;
}

std::vector<ca::u8> gzip_decompress(const std::vector<ca::u8>& data)
{
    return gzip_decompress(data.data(), data.size());
}

std::vector<ca::u8> gzip_decompress(const ca::u8* data, ca::usize size)
{
    GzipReader reader(data, size);
    return reader.read_all();
}

}   // namespace ca::zip
