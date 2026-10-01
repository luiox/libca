#include "libca/zip/gzip_reader.hpp"

#include <cstring>
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

// u8 先提升到 u32 再移位：p[3] << 24 若按 int 提升做有符号移位，p[3] >= 0x80 时
// 溢出为 UB（C++17 下未定义，C++20 才收敛为回绕）。
ca::u16 read_u16(const ca::u8* p) {
    return static_cast<ca::u16>(static_cast<ca::u32>(p[0]) | (static_cast<ca::u32>(p[1]) << 8));
}

ca::u32 read_u32(const ca::u8* p) {
    return static_cast<ca::u32>(p[0]) | (static_cast<ca::u32>(p[1]) << 8) | (static_cast<ca::u32>(p[2]) << 16) |
           (static_cast<ca::u32>(p[3]) << 24);
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

    z_stream zs{};
    bool     zs_init = false;

    Crc32   crc;
    ca::u64 member_uncompressed_size = 0;   // 当前成员累计解压字节数（ISIZE 校验用）

    ~Impl() {
        if (zs_init) {
            ::inflateEnd(&zs);
        }
    }

    void release_zstream() {
        if (!zs_init) {
            return;
        }
        ::inflateEnd(&zs);
        zs_init = false;
    }

    // 解析从 cursor 起的成员头，返回头总长（含可选字段与 FHCRC）。
    // 输入不足以容纳头或任一变长字段按流截断处理。
    Result<ca::usize, ZipErrorInfo> parse_member_header() {
        const ca::u8*   p     = data.data() + cursor;
        const ca::usize avail = data.size() - cursor;

        if (avail < kFixedHeadSize) {
            return Err(ZipErrorInfo{ZipError::INVALID_FORMAT, "truncated gzip header"});
        }
        if (p[0] != kGzipMagic1 || p[1] != kGzipMagic2) {
            return Err(ZipErrorInfo{ZipError::INVALID_FORMAT, "bad gzip magic"});
        }
        if (p[2] != kDeflateCm) {
            return Err(
                ZipErrorInfo{ZipError::UNSUPPORTED, "unsupported gzip compression method: " + std::to_string(p[2])});
        }
        const ca::u8 flg = p[3];
        if ((flg & kFlagReserved) != 0) {
            return Err(ZipErrorInfo{ZipError::UNSUPPORTED, "reserved gzip header flags set"});
        }

        // MTIME/XFL/OS 不参与解压语义，直接跳过。
        ca::usize offset = kFixedHeadSize;

        if ((flg & kFlagFextra) != 0) {
            if (avail < offset + 2) {
                return Err(ZipErrorInfo{ZipError::INVALID_FORMAT, "truncated gzip FEXTRA length"});
            }
            const ca::u16 xlen = read_u16(p + offset);
            offset += 2;
            if (avail < offset + xlen) {
                return Err(ZipErrorInfo{ZipError::INVALID_FORMAT, "truncated gzip FEXTRA data"});
            }
            offset += xlen;
        }
        if ((flg & kFlagFname) != 0) {
            auto skipped = skip_nul_terminated_field(p, avail, offset, "FNAME");
            if (skipped.is_err()) {
                return Err(std::move(skipped).unwrap_err());
            }
            offset = std::move(skipped).unwrap();
        }
        if ((flg & kFlagFcomment) != 0) {
            auto skipped = skip_nul_terminated_field(p, avail, offset, "FCOMMENT");
            if (skipped.is_err()) {
                return Err(std::move(skipped).unwrap_err());
            }
            offset = std::move(skipped).unwrap();
        }

        if ((flg & kFlagFhcrc) != 0) {
            if (avail < offset + 2) {
                return Err(ZipErrorInfo{ZipError::INVALID_FORMAT, "truncated gzip FHCRC"});
            }
            // FHCRC 为头前缀（不含自身）CRC32 的低 16 位。
            const ca::u16 stored_crc16 = read_u16(p + offset);
            Crc32         header_crc;
            header_crc.update(p, offset);
            if (static_cast<ca::u16>(header_crc.value() & 0xFFFFu) != stored_crc16) {
                return Err(ZipErrorInfo{ZipError::CRC_MISMATCH, "gzip header CRC16 mismatch"});
            }
            offset += 2;
        }

        return Ok(offset);
    }

    // 跳过 NUL 结尾的变长字段；越界（缺终止符）按流截断处理。
    static Result<ca::usize, ZipErrorInfo> skip_nul_terminated_field(const ca::u8* p, ca::usize avail, ca::usize offset,
                                                                     const char* field) {
        while (offset < avail) {
            if (p[offset] == 0) {
                return Ok(offset + 1);
            }
            ++offset;
        }
        return Err(ZipErrorInfo{ZipError::INVALID_FORMAT, std::string("truncated gzip ") + field});
    }

    // cursor 处开始新成员：解析头并初始化 raw deflate inflate。
    Result<void, ZipErrorInfo> begin_member() {
        auto headerLength = parse_member_header();
        if (headerLength.is_err()) {
            return Err(std::move(headerLength).unwrap_err());
        }
        cursor += std::move(headerLength).unwrap();

        release_zstream();
        std::memset(&zs, 0, sizeof(zs));
        if (::inflateInit2(&zs, -MAX_WBITS) != Z_OK) {
            return Err(ZipErrorInfo{ZipError::ZLIB_ERROR, "zlib inflateInit2 failed"});
        }
        zs_init                  = true;
        crc                      = Crc32();
        member_uncompressed_size = 0;
        state                    = State::kDeflate;
        return Ok();
    }

    // 校验成员尾并推进状态：有剩余输入继续下一成员，否则完成。
    Result<void, ZipErrorInfo> finish_member() {
        if (data.size() - cursor < kTrailerSize) {
            return Err(ZipErrorInfo{ZipError::INVALID_FORMAT, "truncated gzip trailer"});
        }
        const ca::u32 stored_crc   = read_u32(data.data() + cursor);
        const ca::u32 stored_isize = read_u32(data.data() + cursor + 4);
        cursor += kTrailerSize;

        release_zstream();

        if (crc.value() != stored_crc) {
            return Err(ZipErrorInfo{ZipError::CRC_MISMATCH, "gzip CRC32 mismatch"});
        }
        if (static_cast<ca::u32>(member_uncompressed_size & 0xFFFFFFFFu) != stored_isize) {
            return Err(ZipErrorInfo{ZipError::CRC_MISMATCH, "gzip ISIZE mismatch"});
        }

        state = cursor < data.size() ? State::kHeader : State::kDone;
        return Ok();
    }
};

GzipReader::GzipReader(std::vector<ca::u8> data)
    : impl_(std::make_unique<Impl>()) {
    impl_->data = std::move(data);
}

GzipReader::GzipReader(const ca::u8* data, ca::usize size)
    : GzipReader(size > 0 && data != nullptr ? std::vector<ca::u8>(data, data + size) : std::vector<ca::u8>()) {}

GzipReader::~GzipReader() = default;

Result<int, ZipErrorInfo> GzipReader::read(ca::u8* buffer, ca::usize size) {
    ca::usize filled = 0;

    while (filled < size && impl_->state != Impl::State::kDone) {
        switch (impl_->state) {
        case Impl::State::kHeader: {
            auto begun = impl_->begin_member();
            if (begun.is_err()) {
                return Err(std::move(begun).unwrap_err());
            }
            break;
        }
        case Impl::State::kDeflate: {
            if (impl_->cursor >= impl_->data.size()) {
                return Err(
                    ZipErrorInfo{ZipError::INVALID_FORMAT, "truncated gzip stream: unexpected end of deflate data"});
            }

            auto*            zs       = &impl_->zs;
            size_t           feed     = impl_->data.size() - impl_->cursor;
            constexpr size_t kMaxFeed = static_cast<size_t>(0x7FFFFFFF);
            if (feed > kMaxFeed) {
                feed = kMaxFeed;
            }
            zs->next_in   = const_cast<Bytef*>(reinterpret_cast<const Bytef*>(impl_->data.data() + impl_->cursor));
            zs->avail_in  = static_cast<uInt>(feed);
            zs->next_out  = buffer + filled;
            zs->avail_out = static_cast<uInt>(size - filled);

            const int    ret      = ::inflate(zs, Z_NO_FLUSH);
            const size_t consumed = feed - zs->avail_in;
            const size_t produced = static_cast<size_t>(size - filled) - zs->avail_out;
            impl_->cursor += consumed;
            filled += produced;
            impl_->member_uncompressed_size += produced;
            impl_->crc.update(buffer + filled - produced, produced);

            if (ret == Z_STREAM_END) {
                impl_->state = Impl::State::kTrailer;
            } else if (ret == Z_DATA_ERROR) {
                return Err(
                    ZipErrorInfo{ZipError::INVALID_FORMAT,
                                 "zlib raw inflate failed (" + std::to_string(ret) + "): corrupted deflate data"});
            } else if (ret != Z_OK) {
                return Err(ZipErrorInfo{ZipError::ZLIB_ERROR, "zlib raw inflate failed (" + std::to_string(ret) + ")"});
            }
            break;
        }
        case Impl::State::kTrailer: {
            auto finished = impl_->finish_member();
            if (finished.is_err()) {
                return Err(std::move(finished).unwrap_err());
            }
            break;
        }
        case Impl::State::kDone: {
            break;
        }
        }
    }

    return Ok(static_cast<int>(filled));
}

Result<std::vector<ca::u8>, ZipErrorInfo> GzipReader::read_all() {
    std::vector<ca::u8> out;
    ca::u8              buffer[16384];
    while (true) {
        auto readResult = read(buffer, sizeof(buffer));
        if (readResult.is_err()) {
            return Err(std::move(readResult).unwrap_err());
        }
        const int n = std::move(readResult).unwrap();
        if (n <= 0) {
            break;
        }
        out.insert(out.end(), buffer, buffer + n);
    }
    return Ok(std::move(out));
}

Result<std::vector<ca::u8>, ZipErrorInfo> gzip_decompress(const std::vector<ca::u8>& data) {
    return gzip_decompress(data.data(), data.size());
}

Result<std::vector<ca::u8>, ZipErrorInfo> gzip_decompress(const ca::u8* data, ca::usize size) {
    GzipReader reader(data, size);
    return reader.read_all();
}

}   // namespace ca::zip
