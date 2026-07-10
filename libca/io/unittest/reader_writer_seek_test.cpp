#include <gmock/gmock.h>

#include <algorithm>
#include <cstring>
#include <limits>
#include <string>
#include <utility>
#include <vector>

#include "libca/io/reader.hpp"
#include "libca/io/seek.hpp"
#include "libca/io/writer.hpp"

namespace ca::io::test {
namespace {

class ChunkReader final : public Reader
{
public:
    explicit ChunkReader(std::string data, usize max_chunk = 2, usize interrupts = 0)
        : data_(std::move(data))
        , max_chunk_(max_chunk)
        , interrupts_(interrupts)
    {}

    IoResult<usize> read(u8* buffer, usize capacity) override
    {
        ++read_count;
        if (interrupts_ != 0) {
            --interrupts_;
            return ca::core::Err(
                IoError::from_kind(IoErrorKind::Interrupted, "simulated interrupt"));
        }
        if (position_ == data_.size() || capacity == 0)
            return ca::core::Ok(static_cast<usize>(0));
        const usize count = std::min({capacity, max_chunk_, data_.size() - position_});
        std::memcpy(buffer, data_.data() + position_, count);
        position_ += count;
        return ca::core::Ok(count);
    }

    usize read_count{0};

private:
    std::string data_;
    usize       max_chunk_{0};
    usize       interrupts_{0};
    usize       position_{0};
};

class InvalidCountReader final : public Reader
{
public:
    IoResult<usize> read(u8*, usize capacity) override { return ca::core::Ok(capacity + 1); }
};

class RecordingWriter final : public Writer
{
public:
    explicit RecordingWriter(usize max_chunk = 2, usize interrupts = 0, bool write_zero = false)
        : max_chunk_(max_chunk)
        , interrupts_(interrupts)
        , write_zero_(write_zero)
    {}

    IoResult<usize> write(const u8* data, usize length) override
    {
        ++write_count;
        if (interrupts_ != 0) {
            --interrupts_;
            return ca::core::Err(
                IoError::from_kind(IoErrorKind::Interrupted, "simulated interrupt"));
        }
        if (write_zero_)
            return ca::core::Ok(static_cast<usize>(0));
        const usize count = std::min(length, max_chunk_);
        output.append(reinterpret_cast<const char*>(data), count);
        return ca::core::Ok(count);
    }

    IoResult<void> flush() override
    {
        ++flush_count;
        return ca::core::Ok();
    }

    std::string output;
    usize       write_count{0};
    usize       flush_count{0};

private:
    usize max_chunk_{0};
    usize interrupts_{0};
    bool  write_zero_{false};
};

class MemorySeek final : public Seek
{
public:
    explicit MemorySeek(u64 length)
        : length_(length)
    {}

    IoResult<u64> seek(const SeekFrom& position) override
    {
        i64 next = 0;
        switch (position.origin()) {
        case SeekOrigin::Start:
            if (position.absolute_position() > static_cast<u64>(std::numeric_limits<i64>::max()))
                return ca::core::Err(
                    IoError::from_kind(IoErrorKind::InvalidInput, "position too large"));
            next = static_cast<i64>(position.absolute_position());
            break;
        case SeekOrigin::Current:
            next = static_cast<i64>(position_) + position.relative_offset();
            break;
        case SeekOrigin::End: next = static_cast<i64>(length_) + position.relative_offset(); break;
        }
        if (next < 0)
            return ca::core::Err(
                IoError::from_kind(IoErrorKind::InvalidInput, "negative position"));
        position_ = static_cast<u64>(next);
        return ca::core::Ok(position_);
    }

private:
    u64 length_{0};
    u64 position_{0};
};

TEST(ReaderTest, ReadExactHandlesShortReadsAndInterrupted)
{
    ChunkReader reader("abcdef", 2, 1);
    u8          buffer[6]{};

    auto result = reader.read_exact(buffer, sizeof(buffer));

    ASSERT_TRUE(result.is_ok()) << result.unwrap_err().to_string();
    EXPECT_EQ(std::string(reinterpret_cast<char*>(buffer), sizeof(buffer)), "abcdef");
    EXPECT_EQ(reader.read_count, 4U);
}

TEST(ReaderTest, ReadExactReportsUnexpectedEofAndInvalidBuffer)
{
    ChunkReader reader("abc");
    u8          buffer[4]{};

    auto eof = reader.read_exact(buffer, sizeof(buffer));
    ASSERT_TRUE(eof.is_err());
    EXPECT_EQ(eof.unwrap_err().kind(), IoErrorKind::UnexpectedEof);

    auto invalid = reader.read_exact(nullptr, 1);
    ASSERT_TRUE(invalid.is_err());
    EXPECT_EQ(invalid.unwrap_err().kind(), IoErrorKind::InvalidInput);
    EXPECT_TRUE(reader.read_exact(nullptr, 0).is_ok());
}

TEST(ReaderTest, ReadExactRejectsInvalidReaderCount)
{
    InvalidCountReader reader;
    u8                 value{};

    auto result = reader.read_exact(&value, 1);

    ASSERT_TRUE(result.is_err());
    EXPECT_EQ(result.unwrap_err().kind(), IoErrorKind::InvalidData);
}

TEST(ReaderTest, ReadToEndReturnsBytesAndEnforcesLimit)
{
    ChunkReader reader("abcdef", 3);
    auto        result = reader.read_to_end(6);

    ASSERT_TRUE(result.is_ok()) << result.unwrap_err().to_string();
    auto bytes = std::move(result).unwrap();
    ASSERT_EQ(bytes.remaining(), 6U);
    EXPECT_EQ(std::string(reinterpret_cast<const char*>(bytes.as_ptr()), bytes.remaining()),
              "abcdef");

    ChunkReader too_long("abcdef", 6);
    auto        limited = too_long.read_to_end(5);
    ASSERT_TRUE(limited.is_err());
    EXPECT_EQ(limited.unwrap_err().kind(), IoErrorKind::InvalidData);
}

TEST(WriterTest, WriteAllHandlesShortWritesAndInterrupted)
{
    RecordingWriter   writer(2, 1);
    const std::string value = "abcdef";

    auto result = writer.write_all(reinterpret_cast<const u8*>(value.data()), value.size());

    ASSERT_TRUE(result.is_ok()) << result.unwrap_err().to_string();
    EXPECT_EQ(writer.output, value);
    EXPECT_EQ(writer.write_count, 4U);
}

TEST(WriterTest, WriteAllReportsWriteZeroAndInvalidBuffer)
{
    RecordingWriter writer(2, 0, true);
    const u8        value = 1;

    auto zero = writer.write_all(&value, 1);
    ASSERT_TRUE(zero.is_err());
    EXPECT_EQ(zero.unwrap_err().kind(), IoErrorKind::WriteZero);

    auto invalid = writer.write_all(nullptr, 1);
    ASSERT_TRUE(invalid.is_err());
    EXPECT_EQ(invalid.unwrap_err().kind(), IoErrorKind::InvalidInput);
    EXPECT_TRUE(writer.write_all(nullptr, 0).is_ok());
}

TEST(WriterTest, ByteSliceOverloadWritesAllData)
{
    RecordingWriter writer(8);
    const u8        value[] = {1, 2, 3};

    EXPECT_TRUE(writer.write_all(ca::core::ByteSlice(value, sizeof(value))).is_ok());
    EXPECT_EQ(writer.output, std::string("\x01\x02\x03", 3));
}

TEST(SeekTest, SeekFromPreservesAbsoluteAndRelativeValues)
{
    auto start   = SeekFrom::start(9);
    auto current = SeekFrom::current(-2);
    auto end     = SeekFrom::end(-4);

    EXPECT_EQ(start.origin(), SeekOrigin::Start);
    EXPECT_EQ(start.absolute_position(), 9U);
    EXPECT_EQ(current.origin(), SeekOrigin::Current);
    EXPECT_EQ(current.relative_offset(), -2);
    EXPECT_EQ(end.origin(), SeekOrigin::End);
    EXPECT_EQ(end.relative_offset(), -4);
}

TEST(SeekTest, HelpersReportPositionAndRewind)
{
    MemorySeek stream(100);

    EXPECT_EQ(stream.seek(SeekFrom::start(12)).unwrap(), 12U);
    EXPECT_EQ(stream.stream_position().unwrap(), 12U);
    EXPECT_EQ(stream.seek(SeekFrom::end(-5)).unwrap(), 95U);
    EXPECT_TRUE(stream.rewind().is_ok());
    EXPECT_EQ(stream.stream_position().unwrap(), 0U);
}

}   // namespace
}   // namespace ca::io::test
