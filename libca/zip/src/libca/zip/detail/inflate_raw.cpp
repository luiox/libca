#include "libca/zip/detail/inflate_raw.hpp"

#include <string>

#include <zlib.h>

namespace ca::zip {

Result<std::vector<ca::u8>, ZipErrorInfo> inflate_raw(const ca::u8* data, size_t size, size_t hint_uncompressed_size) {
    z_stream stream{};
    if (::inflateInit2(&stream, -MAX_WBITS) != Z_OK) {
        return Err(ZipErrorInfo{ZipError::ZLIB_ERROR, "zlib inflateInit2 failed"});
    }

    // 预分配按可信提示钳制：头部声明值不可信时避免天文数字预分配，
    // 实际产出以流结束为准，容量不足时循环扩容。
    std::vector<ca::u8> out;
    out.reserve(hint_uncompressed_size > 0 ? hint_uncompressed_size : 4096);

    stream.next_in  = const_cast<Bytef*>(reinterpret_cast<const Bytef*>(data));
    stream.avail_in = static_cast<uInt>(size);

    ca::u8 buffer[8192];
    while (true) {
        stream.next_out  = buffer;
        stream.avail_out = sizeof(buffer);
        const int status = ::inflate(&stream, Z_NO_FLUSH);
        if (status != Z_OK && status != Z_STREAM_END) {
            ::inflateEnd(&stream);
            if (status == Z_DATA_ERROR) {
                return Err(ZipErrorInfo{
                    ZipError::INVALID_FORMAT,
                    "zlib raw inflate failed (" + std::to_string(status) + "): corrupted or incomplete deflate data"});
            }
            return Err(ZipErrorInfo{ZipError::ZLIB_ERROR, "zlib raw inflate failed (" + std::to_string(status) + ")"});
        }
        out.insert(out.end(), buffer, buffer + (sizeof(buffer) - stream.avail_out));
        if (status == Z_STREAM_END) {
            break;
        }
    }

    ::inflateEnd(&stream);
    return Ok(std::move(out));
}

}   // namespace ca::zip
