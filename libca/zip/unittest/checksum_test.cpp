#include <gtest/gtest.h>

#include <string>
#include <vector>

#include <libca/zip/checksum.hpp>
#include <libca/zip/entry.hpp>

namespace {

using ca::zip::Adler32;
using ca::zip::Crc32;

TEST(Crc32Test, KnownVectors)
{
    // 标准 CRC32 校验值（zlib 语义）。
    EXPECT_EQ(Crc32().value(), 0x00000000u);

    Crc32 crc;
    crc.update("123456789", 9);
    EXPECT_EQ(crc.value(), 0xCBF43926u);

    const std::vector<ca::u8> data = {'H', 'e', 'l', 'l', 'o'};
    Crc32                     crc2;
    crc2.update(data.data(), data.size());
    EXPECT_EQ(crc2.value(), 0xF7D18982u);
}

TEST(Crc32Test, IncrementalUpdateMatchesOneShot)
{
    const std::string payload =
        "The quick brown fox jumps over the lazy dog. 0123456789.";
    Crc32 incremental;
    incremental.update(payload.data(), 10);
    incremental.update(payload.data() + 10, payload.size() - 10);

    Crc32 oneShot;
    oneShot.update(payload.data(), payload.size());
    EXPECT_EQ(incremental.value(), oneShot.value());
}

TEST(Crc32Test, ResetRestoresInitialState)
{
    Crc32 crc;
    crc.update("abc", 3);
    crc.reset();
    EXPECT_EQ(crc.value(), 0x00000000u);
}

TEST(Adler32Test, KnownVectors)
{
    Adler32 adler;
    adler.update("Wikipedia", 9);
    EXPECT_EQ(adler.value(), 0x11E60398u);
}

}   // namespace
