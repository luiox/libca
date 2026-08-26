#include <libca/zip/output_stream.hpp>

#include <cstring>
#include <stdexcept>

#include <libca/zip/checksum.hpp>
#include <libca/zip/entry.hpp>
#include <zlib.h>

namespace ca::zip {

namespace {

constexpr ca::u32 kLocSig = 0x04034b50;
constexpr ca::u32 kCenSig = 0x02014b50;
constexpr ca::u32 kEndSig = 0x06054b50;
constexpr ca::u32 kDataDescriptorSig = 0x08074b50;

void write_u16(ca::u8* p, ca::u16 v)
{
    p[0] = static_cast<ca::u8>(v);
    p[1] = static_cast<ca::u8>(v >> 8);
}

void write_u32(ca::u8* p, ca::u32 v)
{
    p[0] = static_cast<ca::u8>(v);
    p[1] = static_cast<ca::u8>(v >> 8);
    p[2] = static_cast<ca::u8>(v >> 16);
    p[3] = static_cast<ca::u8>(v >> 24);
}

}   // anonymous namespace

struct ZipOutputStream::Impl {
    FILE* file   = nullptr;
    bool  opened = false;
    int   level  = Z_DEFAULT_COMPRESSION;

    bool        entry_open = false;
    std::string entry_name;
    ca::u16     entry_method             = 0;
    ca::u32     current_crc32            = 0;
    ca::u32     current_compressed_size  = 0;
    ca::u32     current_uncompressed_size = 0;
    ca::u32     current_loc_offset       = 0;
    ca::u16     entry_flags              = 0;
    void*       zstream                  = nullptr;
    Crc32       crc32_;

    std::vector<ca::u8> cen_data;
    ca::u16             total_entries = 0;

    ~Impl()
    {
        if (zstream) {
            ::deflateEnd(static_cast<z_stream*>(zstream));
            delete static_cast<z_stream*>(zstream);
        }
        if (file) {
            ::fclose(file);
        }
    }

    // 收尾 deflate 流并归还资源；失败时抛异常前先释放，避免句柄泄漏。
    void finish_and_release_zstream()
    {
        auto* stream = static_cast<z_stream*>(zstream);
        int   ret;
        ca::u8 buffer[8192];
        do {
            stream->next_in   = nullptr;
            stream->avail_in  = 0;
            stream->next_out  = buffer;
            stream->avail_out = sizeof(buffer);
            ret               = ::deflate(stream, Z_FINISH);
            if (ret == Z_STREAM_ERROR) {
                ::deflateEnd(stream);
                delete static_cast<z_stream*>(zstream);
                zstream = nullptr;
                throw std::runtime_error("deflate stream error on finish");
            }
            const size_t have = sizeof(buffer) - stream->avail_out;
            if (have > 0) {
                if (::fwrite(buffer, 1, have, file) != have) {
                    ::deflateEnd(stream);
                    delete static_cast<z_stream*>(zstream);
                    zstream = nullptr;
                    throw std::runtime_error("Failed to write final deflated data");
                }
                current_compressed_size += static_cast<ca::u32>(have);
            }
        } while (ret != Z_STREAM_END);

        ::deflateEnd(stream);
        delete static_cast<z_stream*>(zstream);
        zstream = nullptr;
    }
};

ZipOutputStream::ZipOutputStream()
    : impl_(std::make_unique<Impl>())
{}

ZipOutputStream::ZipOutputStream(const std::string& path)
    : impl_(std::make_unique<Impl>())
{
    open(path);
}

ZipOutputStream::~ZipOutputStream()
{
    if (impl_->opened) {
        try {
            close();
        } catch (...) {
            // 析构函数不抛异常
        }
    }
}

void ZipOutputStream::open(const std::string& path)
{
    if (impl_->opened) {
        throw std::runtime_error("ZipOutputStream already open");
    }
    FILE* fp = nullptr;
#ifdef _MSC_VER
    if (::fopen_s(&fp, path.c_str(), "wb") != 0) fp = nullptr;
#else
    fp = std::fopen(path.c_str(), "wb");
#endif
    if (!fp) {
        throw std::runtime_error("Failed to open file for writing: " + path);
    }
    impl_->file   = fp;
    impl_->opened = true;
}

bool ZipOutputStream::is_open() const
{
    return impl_->opened;
}

void ZipOutputStream::close()
{
    if (!impl_->opened) {
        return;
    }
    if (impl_->entry_open) {
        close_entry();
    }

    const ca::u32 cenOffset = static_cast<ca::u32>(::ftell(impl_->file));

    if (!impl_->cen_data.empty()) {
        if (::fwrite(impl_->cen_data.data(), 1, impl_->cen_data.size(), impl_->file) !=
            impl_->cen_data.size()) {
            throw std::runtime_error("Failed to write CEN entries");
        }
    }
    const ca::u32 cenSize = static_cast<ca::u32>(impl_->cen_data.size());

    ca::u8 eocd[22] = {};
    write_u32(eocd, kEndSig);
    write_u16(eocd + 4, 0);
    write_u16(eocd + 6, 0);
    write_u16(eocd + 8, impl_->total_entries);
    write_u16(eocd + 10, impl_->total_entries);
    write_u32(eocd + 12, cenSize);
    write_u32(eocd + 16, cenOffset);
    write_u16(eocd + 20, 0);

    if (::fwrite(eocd, 1, 22, impl_->file) != 22) {
        throw std::runtime_error("Failed to write EOCD");
    }

    ::fclose(impl_->file);
    impl_->file   = nullptr;
    impl_->opened = false;

    impl_->cen_data.clear();
    impl_->cen_data.shrink_to_fit();
    impl_->total_entries = 0;
}

void ZipOutputStream::put_next_entry(const ZipEntry& entry)
{
    if (impl_->entry_open) {
        close_entry();
    }

    impl_->entry_name     = entry.name();
    impl_->entry_method   = entry.compression_method();
    impl_->current_crc32  = 0;
    impl_->crc32_.reset();
    impl_->current_compressed_size   = 0;
    impl_->current_uncompressed_size = 0;
    impl_->entry_flags               = 0x0808;

    impl_->current_loc_offset = static_cast<ca::u32>(::ftell(impl_->file));

    ca::u8 loc[30] = {};
    write_u32(loc, kLocSig);
    write_u16(loc + 4, 20);
    write_u16(loc + 6, impl_->entry_flags);
    write_u16(loc + 8, impl_->entry_method);
    write_u16(loc + 10, 0);
    write_u16(loc + 12, 0);
    write_u32(loc + 14, 0);
    write_u32(loc + 18, 0);
    write_u32(loc + 22, 0);
    write_u16(loc + 26, static_cast<ca::u16>(impl_->entry_name.size()));
    write_u16(loc + 28, 0);

    if (::fwrite(loc, 1, 30, impl_->file) != 30) {
        throw std::runtime_error("Failed to write LOC header");
    }
    if (!impl_->entry_name.empty()) {
        if (::fwrite(impl_->entry_name.data(), 1, impl_->entry_name.size(), impl_->file) !=
            impl_->entry_name.size()) {
            throw std::runtime_error("Failed to write entry name");
        }
    }

    if (entry.is_deflated()) {
        auto* zs  = new z_stream {};
        const int ret =
            ::deflateInit2(zs, impl_->level, Z_DEFLATED, -15, 8, Z_DEFAULT_STRATEGY);
        if (ret != Z_OK) {
            delete zs;
            throw std::runtime_error("deflateInit2 failed");
        }
        impl_->zstream = zs;
    }

    impl_->entry_open = true;
}

void ZipOutputStream::write(const ca::u8* data, size_t size)
{
    if (!impl_->entry_open) {
        throw std::runtime_error("No entry to write to");
    }
    if (size == 0) return;

    impl_->crc32_.update(data, size);
    impl_->current_uncompressed_size += static_cast<ca::u32>(size);

    if (impl_->entry_method == 0) {
        if (::fwrite(data, 1, size, impl_->file) != size) {
            throw std::runtime_error("Failed to write stored data");
        }
        impl_->current_compressed_size += static_cast<ca::u32>(size);
    } else {
        auto* stream     = static_cast<z_stream*>(impl_->zstream);
        stream->next_in  = const_cast<ca::u8*>(data);
        stream->avail_in = static_cast<uInt>(size);

        ca::u8 buffer[8192];
        do {
            stream->next_out  = buffer;
            stream->avail_out = sizeof(buffer);
            const int ret     = ::deflate(stream, Z_NO_FLUSH);
            if (ret == Z_STREAM_ERROR) {
                throw std::runtime_error("deflate stream error");
            }
            const size_t have = sizeof(buffer) - stream->avail_out;
            if (have > 0) {
                if (::fwrite(buffer, 1, have, impl_->file) != have) {
                    throw std::runtime_error("Failed to write deflated data");
                }
                impl_->current_compressed_size += static_cast<ca::u32>(have);
            }
        } while (stream->avail_out == 0);
    }
}

void ZipOutputStream::write(const std::vector<ca::u8>& data)
{
    write(data.data(), data.size());
}

void ZipOutputStream::close_entry()
{
    if (!impl_->entry_open) {
        return;
    }

    if (impl_->entry_method == 8) {
        impl_->finish_and_release_zstream();
    }

    impl_->current_crc32 = impl_->crc32_.value();

    ca::u8 dd[16] = {};
    write_u32(dd, kDataDescriptorSig);
    write_u32(dd + 4, impl_->current_crc32);
    write_u32(dd + 8, impl_->current_compressed_size);
    write_u32(dd + 12, impl_->current_uncompressed_size);

    if (::fwrite(dd, 1, 16, impl_->file) != 16) {
        throw std::runtime_error("Failed to write data descriptor");
    }

    ca::u8 cen[46] = {};
    write_u32(cen, kCenSig);
    write_u16(cen + 4, 20);
    write_u16(cen + 6, 20);
    write_u16(cen + 8, impl_->entry_flags);
    write_u16(cen + 10, impl_->entry_method);
    write_u16(cen + 12, 0);
    write_u16(cen + 14, 0);
    write_u32(cen + 16, impl_->current_crc32);
    write_u32(cen + 20, impl_->current_compressed_size);
    write_u32(cen + 24, impl_->current_uncompressed_size);
    write_u16(cen + 28, static_cast<ca::u16>(impl_->entry_name.size()));
    write_u16(cen + 30, 0);
    write_u16(cen + 32, 0);
    write_u16(cen + 34, 0);
    write_u16(cen + 36, 0);
    write_u32(cen + 38, 0);
    write_u32(cen + 42, impl_->current_loc_offset);

    impl_->cen_data.insert(impl_->cen_data.end(), cen, cen + 46);
    if (!impl_->entry_name.empty()) {
        impl_->cen_data.insert(
            impl_->cen_data.end(),
            reinterpret_cast<const ca::u8*>(impl_->entry_name.data()),
            reinterpret_cast<const ca::u8*>(impl_->entry_name.data()) +
                impl_->entry_name.size());
    }

    impl_->total_entries++;

    impl_->entry_open                 = false;
    impl_->entry_name.clear();
    impl_->entry_method               = 0;
    impl_->current_crc32              = 0;
    impl_->current_compressed_size    = 0;
    impl_->current_uncompressed_size  = 0;
    impl_->current_loc_offset         = 0;
    impl_->entry_flags                = 0;
}

void ZipOutputStream::set_level(int level)
{
    if (level < -1 || level > 9) {
        throw std::runtime_error("Invalid compression level: " + std::to_string(level));
    }
    impl_->level = level;
}

}   // namespace ca::zip
