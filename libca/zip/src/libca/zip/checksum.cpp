#include "libca/zip/checksum.hpp"

#include <zlib.h>

namespace ca::zip {

Crc32::Crc32()
    : crc_(::crc32(0L, Z_NULL, 0))
{}

void Crc32::update(const void* data, size_t size)
{
    crc_ = static_cast<ca::u32>(
        ::crc32(crc_, static_cast<const Bytef*>(data), static_cast<uInt>(size)));
}

ca::u32 Crc32::value() const
{
    return crc_;
}

void Crc32::reset()
{
    crc_ = ::crc32(0L, Z_NULL, 0);
}

Adler32::Adler32()
    : adler_(::adler32(0L, Z_NULL, 0))
{}

void Adler32::update(const void* data, size_t size)
{
    adler_ = static_cast<ca::u32>(
        ::adler32(adler_, static_cast<const Bytef*>(data), static_cast<uInt>(size)));
}

ca::u32 Adler32::value() const
{
    return adler_;
}

void Adler32::reset()
{
    adler_ = ::adler32(0L, Z_NULL, 0);
}

}   // namespace ca::zip
