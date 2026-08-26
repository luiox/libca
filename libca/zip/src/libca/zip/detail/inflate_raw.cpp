#include <libca/zip/detail/inflate_raw.hpp>

#include <cstring>
#include <stdexcept>
#include <string>

#include <zlib.h>

namespace ca::zip {

std::vector<ca::u8> inflate_raw(const ca::u8* data, size_t size, size_t hint_uncompressed_size)
{
    z_stream stream {};
    if (::inflateInit2(&stream, -MAX_WBITS) != Z_OK) {
        throw std::runtime_error("zlib inflateInit2 failed");
    }

    // 预分配按可信提示钳制：头部声明值不可信时避免天文数字预分配，
    // 实际产出以流结束为准，容量不足时循环扩容。
    std::vector<ca::u8> out;
    out.reserve(hint_uncompressed_size > 0 ? hint_uncompressed_size : 4096);

    try {
        stream.next_in  = const_cast<Bytef*>(reinterpret_cast<const Bytef*>(data));
        stream.avail_in = static_cast<uInt>(size);

        ca::u8 buffer[8192];
        while (true) {
            stream.next_out  = buffer;
            stream.avail_out = sizeof(buffer);
            const int status = ::inflate(&stream, Z_NO_FLUSH);
            if (status != Z_OK && status != Z_STREAM_END) {
                throw std::runtime_error("zlib raw inflate failed (" + std::to_string(status) +
                                         ")");
            }
            out.insert(out.end(), buffer, buffer + (sizeof(buffer) - stream.avail_out));
            if (status == Z_STREAM_END) {
                break;
            }
        }
    }
    catch (...) {
        ::inflateEnd(&stream);
        throw;
    }

    ::inflateEnd(&stream);
    return out;
}

}   // namespace ca::zip
