#include <gtest/gtest.h>

#include <algorithm>
#include <cstdint>
#include <random>
#include <stdexcept>
#include <string>
#include <vector>

#include "libca/zip/gzip_reader.hpp"
#include "libca/zip/gzip_writer.hpp"
#include "zip_fixtures.hpp"

namespace {

using ca::zip::GzipReader;
using ca::zip::GzipWriter;
using zip_test::append_u16_le;
using zip_test::append_u32_le;
using zip_test::crc32_of;

// RFC 1952 FLG 位。
constexpr uint8_t kFlagFtext    = 0x01;
constexpr uint8_t kFlagFhcrc    = 0x02;
constexpr uint8_t kFlagFextra   = 0x04;
constexpr uint8_t kFlagFname    = 0x08;
constexpr uint8_t kFlagFcomment = 0x10;

std::vector<ca::u8> bytes_of(const std::string& text)
{
    return std::vector<ca::u8>(text.begin(), text.end());
}

std::string text_of(const std::vector<ca::u8>& data)
{
    return std::string(data.begin(), data.end());
}

std::vector<ca::u8> bytes_from_hex(const std::string& hex)
{
    auto nibble = [](char c) -> int {
        if (c >= '0' && c <= '9') return c - '0';
        if (c >= 'a' && c <= 'f') return c - 'a' + 10;
        return c - 'A' + 10;
    };
    std::vector<ca::u8> out;
    out.reserve(hex.size() / 2);
    for (size_t i = 0; i + 1 < hex.size(); i += 2) {
        out.push_back(static_cast<ca::u8>((nibble(hex[i]) << 4) | nibble(hex[i + 1])));
    }
    return out;
}

// 确定性伪随机字节（PRNG 生成，模拟高熵不可压数据）。
std::vector<ca::u8> random_bytes(size_t size, uint32_t seed)
{
    std::mt19937                      rng(seed);
    std::uniform_int_distribution<int> dist(0, 255);
    std::vector<ca::u8>               out(size);
    for (size_t i = 0; i < size; ++i) {
        out[i] = static_cast<ca::u8>(dist(rng));
    }
    return out;
}

// 外部工具（Python gzip, mtime=0, level 9）生成的 known-good 单成员流：
// gzip.compress(b'hello world hello world', mtime=0)。
constexpr const char* kPythonSingleHex =
    "1f8b08000000000002ffcb48cdc9c95728cf2fca4951c840b0013bcee2ea17000000";

// 外部工具生成的双成员拼接流：compress(b'ALPHA|') + compress(b'BETA part two')。
constexpr const char* kPythonMultiHex =
    "1f8b08000000000002ff73f409f070ac0100ab5d90ed06000000"
    "1f8b08000000000002ff73720d715428482c2a512829cf070057221bc40d000000";

TEST(GzipRoundTripTest, EmptyData)
{
    const std::vector<ca::u8> original;
    const auto                compressed = ca::zip::gzip_compress(original);
    EXPECT_FALSE(compressed.empty());
    EXPECT_EQ(ca::zip::gzip_decompress(compressed), original);
}

TEST(GzipRoundTripTest, SingleByte)
{
    const std::vector<ca::u8> original = {0x7A};
    const auto                compressed = ca::zip::gzip_compress(original);
    EXPECT_EQ(ca::zip::gzip_decompress(compressed), original);
}

TEST(GzipRoundTripTest, RepeatedTextSeveralKb)
{
    const std::string        sentence = "the quick brown fox jumps over the lazy dog; ";
    std::string              text;
    while (text.size() < 4096) {
        text += sentence;
    }
    const auto original   = bytes_of(text);
    const auto compressed = ca::zip::gzip_compress(original);
    // 重复文本应明显压缩。
    EXPECT_LT(compressed.size(), original.size() / 4);
    EXPECT_EQ(ca::zip::gzip_decompress(compressed), original);
}

TEST(GzipRoundTripTest, Incompressible100Kb)
{
    const auto original   = random_bytes(100 * 1024, 20240501u);
    const auto compressed = ca::zip::gzip_compress(original);
    EXPECT_EQ(ca::zip::gzip_decompress(compressed), original);
}

TEST(GzipRoundTripTest, ChunkedWriterMatchesOneShot)
{
    const auto original = random_bytes(33 * 1024 + 17, 7u);

    GzipWriter writer;
    size_t     offset = 0;
    size_t     chunk  = 1;
    while (offset < original.size()) {
        const size_t n = std::min(chunk, original.size() - offset);
        writer.write(original.data() + offset, n);
        offset += n;
        chunk = chunk * 3 + 5;
    }
    writer.finish();

    EXPECT_EQ(ca::zip::gzip_decompress(writer.output()), original);
    EXPECT_EQ(writer.output(), ca::zip::gzip_compress(original));

    // 相同输入与级别产出确定字节序列。
    EXPECT_EQ(ca::zip::gzip_compress(original), ca::zip::gzip_compress(original));
}

TEST(GzipInteropTest, DecompressPythonGeneratedSingleMember)
{
    const auto gzip_bytes = bytes_from_hex(kPythonSingleHex);
    EXPECT_EQ(text_of(ca::zip::gzip_decompress(gzip_bytes)), "hello world hello world");

    GzipReader reader(gzip_bytes);
    EXPECT_EQ(reader.read_all(), bytes_of("hello world hello world"));
}

TEST(GzipInteropTest, OutputHeaderMatchesPythonLayout)
{
    // level 9 下 XFL=2、OS=0xFF、MTIME=0，与 Python gzip.compress(…, mtime=0)
    // 的 10 字节头一致。
    const auto ours   = ca::zip::gzip_compress(bytes_of("hello world hello world"), 9);
    const auto python = bytes_from_hex(kPythonSingleHex);
    ASSERT_GE(ours.size(), 10u);
    EXPECT_EQ(std::vector<ca::u8>(ours.begin(), ours.begin() + 10),
              std::vector<ca::u8>(python.begin(), python.begin() + 10));
}

TEST(GzipMultiMemberTest, ConcatenatedStreamFromTool)
{
    const auto gzip_bytes = bytes_from_hex(kPythonMultiHex);
    EXPECT_EQ(text_of(ca::zip::gzip_decompress(gzip_bytes)), "ALPHA|BETA part two");
}

TEST(GzipMultiMemberTest, ConcatenatedOwnMembers)
{
    const auto first  = ca::zip::gzip_compress(bytes_of("first member|"));
    const auto second = ca::zip::gzip_compress(bytes_of("second member"));

    std::vector<ca::u8> joined = first;
    joined.insert(joined.end(), second.begin(), second.end());

    EXPECT_EQ(text_of(ca::zip::gzip_decompress(joined)), "first member|second member");

    // 小缓冲分片读也应在成员边界自动续读。
    GzipReader          reader(joined);
    std::vector<ca::u8> out;
    ca::u8              buffer[5];
    while (true) {
        const int n = reader.read(buffer, sizeof(buffer));
        if (n <= 0) break;
        out.insert(out.end(), buffer, buffer + n);
    }
    EXPECT_EQ(text_of(out), "first member|second member");
    // 全部成员耗尽后再读返回 0。
    EXPECT_EQ(reader.read(buffer, sizeof(buffer)), 0);
}

TEST(GzipErrorTest, BadMagicThrows)
{
    auto data = ca::zip::gzip_compress(bytes_of("payload"));
    data[0] = 0x1E;
    EXPECT_THROW(ca::zip::gzip_decompress(data), std::runtime_error);

    data[0] = 0x1F;
    data[1] = 0x00;
    EXPECT_THROW(ca::zip::gzip_decompress(data), std::runtime_error);
}

TEST(GzipErrorTest, UnsupportedMethodThrows)
{
    auto data = ca::zip::gzip_compress(bytes_of("payload"));
    data[2] = 9;
    EXPECT_THROW(ca::zip::gzip_decompress(data), std::runtime_error);
}

TEST(GzipErrorTest, ReservedHeaderFlagsThrow)
{
    auto data = ca::zip::gzip_compress(bytes_of("payload"));
    data[3] |= 0x40;
    EXPECT_THROW(ca::zip::gzip_decompress(data), std::runtime_error);
}

TEST(GzipErrorTest, TruncatedHeaderThrows)
{
    const auto data = ca::zip::gzip_compress(bytes_of("payload"));
    EXPECT_THROW(ca::zip::gzip_decompress(data.data(), 5), std::runtime_error);
    EXPECT_THROW(ca::zip::gzip_decompress(std::vector<ca::u8> {}), std::runtime_error);
}

TEST(GzipErrorTest, TruncatedDeflateBodyThrows)
{
    auto data = ca::zip::gzip_compress(bytes_of("payload for truncation"));
    ASSERT_GT(data.size(), 20u);
    data.resize(data.size() - 20);
    EXPECT_THROW(ca::zip::gzip_decompress(data), std::runtime_error);
}

TEST(GzipErrorTest, TruncatedTrailerThrows)
{
    auto data = ca::zip::gzip_compress(bytes_of("payload"));
    data.resize(data.size() - 4);   // 截掉 ISIZE 一半之后的部分
    EXPECT_THROW(ca::zip::gzip_decompress(data), std::runtime_error);
}

TEST(GzipErrorTest, CorruptedTrailerCrcThrows)
{
    auto data = ca::zip::gzip_compress(bytes_of("payload with crc trailer"));
    ASSERT_GE(data.size(), 8u);
    data[data.size() - 8] ^= 0xFF;   // 尾部 CRC32 首字节翻转
    EXPECT_THROW(ca::zip::gzip_decompress(data), std::runtime_error);
}

TEST(GzipErrorTest, CorruptedTrailerIsizeThrows)
{
    auto data = ca::zip::gzip_compress(bytes_of("payload with isize trailer"));
    ASSERT_GE(data.size(), 4u);
    data[data.size() - 1] ^= 0x01;   // ISIZE 末字节翻转
    EXPECT_THROW(ca::zip::gzip_decompress(data), std::runtime_error);
}

TEST(GzipErrorTest, CorruptedDeflateBodyThrows)
{
    auto data = ca::zip::gzip_compress(bytes_of("payload whose body will be corrupted"));
    ASSERT_GT(data.size(), 20u);
    data[15] ^= 0xFF;   // deflate 数据段中间翻转
    EXPECT_THROW(ca::zip::gzip_decompress(data), std::runtime_error);
}

TEST(GzipErrorTest, TruncatedFnameThrows)
{
    auto compressed = ca::zip::gzip_compress(bytes_of("payload"));
    std::vector<ca::u8> data(compressed.begin(), compressed.begin() + 10);
    data[3] = kFlagFname;
    data.push_back('n');   // 有名字字节但没有 NUL 终止符，流即结束
    EXPECT_THROW(ca::zip::gzip_decompress(data), std::runtime_error);
}

TEST(GzipHeaderFlagsTest, FtextOnlyFlagAccepted)
{
    auto data = ca::zip::gzip_compress(bytes_of("plain text flag"));
    data[3] = kFlagFtext;   // FTEXT 为纯信息位，不带附加字段
    EXPECT_EQ(ca::zip::gzip_decompress(data), bytes_of("plain text flag"));
}

TEST(GzipHeaderFlagsTest, AllOptionalFieldsParsed)
{
    const std::string         name    = "frame.log";
    const std::string         comment = "generated by unit test";
    const std::vector<ca::u8> extra   = {0x01, 0x02, 0x03, 0x04};
    const auto                payload = bytes_of("all flags payload");

    auto compressed = ca::zip::gzip_compress(payload);

    // 手工拼出 FTEXT|FHCRC|FEXTRA|FNAME|FCOMMENT 全开的新头，deflate 体与尾不变。
    std::vector<ca::u8> data;
    data.push_back(0x1F);
    data.push_back(0x8B);
    data.push_back(8);
    data.push_back(kFlagFtext | kFlagFhcrc | kFlagFextra | kFlagFname | kFlagFcomment);
    append_u32_le(data, 0);          // MTIME
    data.push_back(0);               // XFL
    data.push_back(0xFF);            // OS
    append_u16_le(data, static_cast<uint16_t>(extra.size()));
    data.insert(data.end(), extra.begin(), extra.end());
    data.insert(data.end(), name.begin(), name.end());
    data.push_back(0);
    data.insert(data.end(), comment.begin(), comment.end());
    data.push_back(0);
    append_u16_le(data, static_cast<uint16_t>(crc32_of(data) & 0xFFFFu));   // FHCRC
    data.insert(data.end(), compressed.begin() + 10, compressed.end());

    EXPECT_EQ(ca::zip::gzip_decompress(data), payload);
}

TEST(GzipHeaderFlagsTest, HeaderCrcMismatchThrows)
{
    auto compressed = ca::zip::gzip_compress(bytes_of("payload"));
    std::vector<ca::u8> data(compressed.begin(), compressed.begin() + 10);
    data[3] = kFlagFhcrc;
    append_u16_le(data, 0xBEEF);   // 错误的 CRC16
    data.insert(data.end(), compressed.begin() + 10, compressed.end());
    EXPECT_THROW(ca::zip::gzip_decompress(data), std::runtime_error);
}

TEST(GzipWriterTest, WritesHeaderAndTrailer)
{
    GzipWriter writer;
    writer.write(bytes_of("hello"));
    writer.finish();

    const auto& out = writer.output();
    ASSERT_GE(out.size(), 18u);
    EXPECT_EQ(out[0], 0x1F);
    EXPECT_EQ(out[1], 0x8B);
    EXPECT_EQ(out[2], 8);    // CM = deflate
    EXPECT_EQ(out[3], 0);    // FLG = 0
    EXPECT_EQ(out[8], 0);    // 默认级别 XFL = 0
    // 尾部 ISIZE = 5。
    EXPECT_EQ(out[out.size() - 4], 5);
    EXPECT_EQ(out[out.size() - 3], 0);
    EXPECT_EQ(out[out.size() - 2], 0);
    EXPECT_EQ(out[out.size() - 1], 0);

    EXPECT_TRUE(writer.finished());
}

TEST(GzipWriterTest, XflReflectsLevel)
{
    GzipWriter fast(1);
    GzipWriter best(9);
    EXPECT_EQ(fast.output()[8], 4);
    EXPECT_EQ(best.output()[8], 2);
}

TEST(GzipWriterTest, LevelStillAppliedAfterHeader)
{
    // 头在构造时已写，构造后仍应以指定级别压缩。
    GzipWriter writer(9);
    const auto original = bytes_of(std::string(2048, 'a'));
    writer.write(original);
    writer.finish();
    EXPECT_EQ(ca::zip::gzip_decompress(writer.output()), original);
    EXPECT_EQ(writer.output(), ca::zip::gzip_compress(original, 9));
}

TEST(GzipWriterTest, FinishIsIdempotentAndWriteAfterFinishThrows)
{
    GzipWriter writer;
    writer.write(bytes_of("data"));
    writer.finish();
    const auto size_before = writer.output().size();
    writer.finish();
    EXPECT_EQ(writer.output().size(), size_before);
    EXPECT_THROW(writer.write(bytes_of("more")), std::runtime_error);
}

TEST(GzipWriterTest, TakeMovesOutputOut)
{
    GzipWriter writer;
    writer.write(bytes_of("payload"));
    writer.finish();
    auto taken = writer.take();
    EXPECT_TRUE(writer.output().empty());
    EXPECT_EQ(text_of(ca::zip::gzip_decompress(taken)), "payload");
}

TEST(GzipWriterTest, EmptyWriteIsIgnored)
{
    GzipWriter writer;
    writer.write(nullptr, 0);
    writer.write(std::vector<ca::u8> {});
    writer.finish();
    EXPECT_EQ(text_of(ca::zip::gzip_decompress(writer.output())), "");
}

TEST(GzipWriterTest, InvalidLevelThrows)
{
    EXPECT_THROW(GzipWriter(10), std::runtime_error);
    EXPECT_THROW(GzipWriter(-2), std::runtime_error);
    EXPECT_THROW(static_cast<void>(ca::zip::gzip_compress(bytes_of("x"), 42)),
                 std::runtime_error);
}

TEST(GzipReaderTest, ChunkedSmallBufferReadsMatchWhole)
{
    const auto original   = bytes_of(std::string(4096, '=') + "tail");
    const auto compressed = ca::zip::gzip_compress(original);

    GzipReader          reader(compressed);
    std::vector<ca::u8> out;
    ca::u8              buffer[7];
    while (true) {
        const int n = reader.read(buffer, sizeof(buffer));
        if (n <= 0) break;
        out.insert(out.end(), buffer, buffer + n);
    }
    EXPECT_EQ(out, original);
}

TEST(GzipReaderTest, ReadIntoLargeBufferReturnsFullPayload)
{
    const auto original   = random_bytes(50 * 1024, 99u);
    const auto compressed = ca::zip::gzip_compress(original);

    GzipReader reader(compressed);
    auto       out = reader.read_all();
    EXPECT_EQ(out, original);
    // 读尽后继续读返回 0。
    ca::u8 buffer[16];
    EXPECT_EQ(reader.read(buffer, sizeof(buffer)), 0);
}

}   // namespace
