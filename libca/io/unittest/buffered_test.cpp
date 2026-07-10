#include <gmock/gmock.h>

#include <algorithm>
#include <cstring>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "libca/io/buffered.hpp"

namespace ca::io::test {
namespace {

class TrackingReader final : public Reader
{
public:
    explicit TrackingReader(std::string input)
        : input_(std::move(input))
    {}

    IoResult<usize> read(u8* buffer, usize capacity) override
    {
        requests.push_back(capacity);
        if (position_ == input_.size())
            return ca::core::Ok(static_cast<usize>(0));
        const usize count = std::min(capacity, input_.size() - position_);
        std::memcpy(buffer, input_.data() + position_, count);
        position_ += count;
        return ca::core::Ok(count);
    }

    std::vector<usize> requests;

private:
    std::string input_;
    usize       position_{0};
};

struct WriterState
{
    std::string output;
    usize       write_count{0};
    usize       flush_count{0};
    usize       max_chunk{1024};
    usize       fail_on_call{0};
};

class SharedWriter final : public Writer
{
public:
    explicit SharedWriter(std::shared_ptr<WriterState> state)
        : state_(std::move(state))
    {}

    IoResult<usize> write(const u8* data, usize length) override
    {
        ++state_->write_count;
        if (state_->fail_on_call == state_->write_count)
            return ca::core::Err(IoError::from_kind(IoErrorKind::Other, "simulated write failure"));
        const usize count = std::min(length, state_->max_chunk);
        state_->output.append(reinterpret_cast<const char*>(data), count);
        return ca::core::Ok(count);
    }

    IoResult<void> flush() override
    {
        ++state_->flush_count;
        return ca::core::Ok();
    }

private:
    std::shared_ptr<WriterState> state_;
};

TEST(BufReaderTest, RejectsInvalidConstruction)
{
    auto no_reader = BufReader::create(nullptr, 4);
    ASSERT_TRUE(no_reader.is_err());
    EXPECT_EQ(no_reader.unwrap_err().kind(), IoErrorKind::InvalidInput);

    auto zero_capacity = BufReader::create(std::make_unique<TrackingReader>("data"), 0);
    ASSERT_TRUE(zero_capacity.is_err());
    EXPECT_EQ(zero_capacity.unwrap_err().kind(), IoErrorKind::InvalidInput);
}

TEST(BufReaderTest, ReadsAcrossBufferAndBypassesForLargeRequest)
{
    auto  source  = std::make_unique<TrackingReader>("abcdefgh");
    auto* raw     = source.get();
    auto  created = BufReader::create(std::move(source), 4);
    ASSERT_TRUE(created.is_ok());
    auto reader = std::move(created).unwrap();
    u8   first[2]{};
    u8   second[2]{};
    u8   large[4]{};

    EXPECT_EQ(reader.read(first, sizeof(first)).unwrap(), 2U);
    EXPECT_EQ(reader.buffered_len(), 2U);
    EXPECT_EQ(reader.read(second, sizeof(second)).unwrap(), 2U);
    EXPECT_EQ(reader.read(large, sizeof(large)).unwrap(), 4U);

    EXPECT_EQ(std::string(reinterpret_cast<char*>(first), 2), "ab");
    EXPECT_EQ(std::string(reinterpret_cast<char*>(second), 2), "cd");
    EXPECT_EQ(std::string(reinterpret_cast<char*>(large), 4), "efgh");
    ASSERT_EQ(raw->requests.size(), 2U);
    EXPECT_EQ(raw->requests[0], 4U);
    EXPECT_EQ(raw->requests[1], 4U);
}

TEST(BufReaderTest, FillConsumeAndReadUntilPreserveOrder)
{
    auto created = BufReader::create(std::make_unique<TrackingReader>("ab\ncd\n"), 3);
    ASSERT_TRUE(created.is_ok());
    auto reader = std::move(created).unwrap();

    auto view = reader.fill_buf();
    ASSERT_TRUE(view.is_ok());
    EXPECT_EQ(
        std::string(reinterpret_cast<const char*>(view.unwrap().data()), view.unwrap().size()),
        "ab\n");
    EXPECT_TRUE(reader.consume(1).is_ok());
    EXPECT_EQ(reader.buffered_len(), 2U);
    auto too_far = reader.consume(3);
    ASSERT_TRUE(too_far.is_err());
    EXPECT_EQ(too_far.unwrap_err().kind(), IoErrorKind::InvalidInput);

    auto output = ca::core::BytesMut::with_capacity(8);
    EXPECT_EQ(reader.read_until(static_cast<u8>('\n'), output).unwrap(), 2U);
    EXPECT_EQ(reader.read_until(static_cast<u8>('\n'), output).unwrap(), 3U);
    EXPECT_EQ(std::string(reinterpret_cast<const char*>(output.as_ptr()), output.remaining()),
              "b\ncd\n");
}

TEST(BufReaderTest, MoveAndIntoInnerTransferOwnership)
{
    auto created = BufReader::create(std::make_unique<TrackingReader>("x"), 2);
    ASSERT_TRUE(created.is_ok());
    auto source = std::move(created).unwrap();
    auto target = std::move(source);

    EXPECT_EQ(source.inner(), nullptr);
    auto inner = target.into_inner();
    EXPECT_NE(inner, nullptr);
    EXPECT_EQ(target.inner(), nullptr);
    u8   value{};
    auto moved = target.read(&value, 1);
    ASSERT_TRUE(moved.is_err());
    EXPECT_EQ(moved.unwrap_err().kind(), IoErrorKind::InvalidInput);
}

TEST(BufWriterTest, RejectsInvalidConstruction)
{
    auto no_writer = BufWriter::create(nullptr, 4);
    ASSERT_TRUE(no_writer.is_err());
    EXPECT_EQ(no_writer.unwrap_err().kind(), IoErrorKind::InvalidInput);

    auto zero_capacity =
        BufWriter::create(std::make_unique<SharedWriter>(std::make_shared<WriterState>()), 0);
    ASSERT_TRUE(zero_capacity.is_err());
    EXPECT_EQ(zero_capacity.unwrap_err().kind(), IoErrorKind::InvalidInput);
}

TEST(BufWriterTest, AggregatesSmallWritesAndFlushesUnderlyingWriter)
{
    auto state   = std::make_shared<WriterState>();
    auto created = BufWriter::create(std::make_unique<SharedWriter>(state), 4);
    ASSERT_TRUE(created.is_ok());
    auto     writer   = std::move(created).unwrap();
    const u8 first[]  = {'a', 'b'};
    const u8 second[] = {'c', 'd'};

    EXPECT_EQ(writer.write(first, sizeof(first)).unwrap(), 2U);
    EXPECT_EQ(writer.write(second, sizeof(second)).unwrap(), 2U);
    EXPECT_EQ(state->write_count, 0U);
    EXPECT_EQ(writer.buffered_len(), 4U);
    EXPECT_TRUE(writer.flush().is_ok());
    EXPECT_EQ(state->output, "abcd");
    EXPECT_EQ(state->flush_count, 1U);
    EXPECT_EQ(writer.buffered_len(), 0U);
}

TEST(BufWriterTest, LargeWriteBypassesEmptyBuffer)
{
    auto state   = std::make_shared<WriterState>();
    auto created = BufWriter::create(std::make_unique<SharedWriter>(state), 4);
    ASSERT_TRUE(created.is_ok());
    auto              writer = std::move(created).unwrap();
    const std::string value  = "abcdefgh";

    EXPECT_EQ(writer.write(reinterpret_cast<const u8*>(value.data()), value.size()).unwrap(),
              value.size());
    EXPECT_EQ(state->output, value);
    EXPECT_EQ(writer.buffered_len(), 0U);
}

TEST(BufWriterTest, RetainsUnwrittenSuffixAfterPartialWriteError)
{
    auto state          = std::make_shared<WriterState>();
    state->max_chunk    = 2;
    state->fail_on_call = 2;
    auto created        = BufWriter::create(std::make_unique<SharedWriter>(state), 8);
    ASSERT_TRUE(created.is_ok());
    auto              writer = std::move(created).unwrap();
    const std::string value  = "abcd";
    ASSERT_TRUE(writer.write(reinterpret_cast<const u8*>(value.data()), value.size()).is_ok());

    auto failed = writer.flush();
    ASSERT_TRUE(failed.is_err());
    EXPECT_EQ(state->output, "ab");
    EXPECT_EQ(writer.buffered_len(), 2U);

    state->fail_on_call = 0;
    EXPECT_TRUE(writer.flush().is_ok());
    EXPECT_EQ(state->output, value);
    EXPECT_EQ(writer.buffered_len(), 0U);
}

TEST(BufWriterTest, FinishFlushesAndReturnsInnerWriter)
{
    auto state   = std::make_shared<WriterState>();
    auto created = BufWriter::create(std::make_unique<SharedWriter>(state), 8);
    ASSERT_TRUE(created.is_ok());
    auto     writer  = std::move(created).unwrap();
    const u8 value[] = {'o', 'k'};
    ASSERT_TRUE(writer.write(value, sizeof(value)).is_ok());

    auto finished = writer.finish();
    ASSERT_TRUE(finished.is_ok()) << finished.unwrap_err().to_string();
    EXPECT_NE(std::move(finished).unwrap(), nullptr);
    EXPECT_EQ(writer.inner(), nullptr);
    EXPECT_EQ(state->output, "ok");
    EXPECT_EQ(state->flush_count, 1U);
}

TEST(BufWriterTest, FinishFailureRetainsWriterAndBufferedSuffix)
{
    auto state          = std::make_shared<WriterState>();
    state->max_chunk    = 2;
    state->fail_on_call = 2;
    auto created        = BufWriter::create(std::make_unique<SharedWriter>(state), 8);
    ASSERT_TRUE(created.is_ok());
    auto              writer = std::move(created).unwrap();
    const std::string value  = "abcd";
    ASSERT_TRUE(writer.write(reinterpret_cast<const u8*>(value.data()), value.size()).is_ok());

    auto failed = writer.finish();
    ASSERT_TRUE(failed.is_err());
    EXPECT_NE(writer.inner(), nullptr);
    EXPECT_EQ(writer.buffered_len(), 2U);
    EXPECT_EQ(state->output, "ab");

    state->fail_on_call = 0;
    auto recovered      = writer.finish();
    ASSERT_TRUE(recovered.is_ok()) << recovered.unwrap_err().to_string();
    EXPECT_EQ(state->output, value);
    EXPECT_EQ(writer.inner(), nullptr);
}

TEST(BufWriterTest, DestructorBestEffortWritesBufferedData)
{
    auto state = std::make_shared<WriterState>();
    {
        auto created = BufWriter::create(std::make_unique<SharedWriter>(state), 8);
        ASSERT_TRUE(created.is_ok());
        auto     writer  = std::move(created).unwrap();
        const u8 value[] = {'o', 'k'};
        ASSERT_TRUE(writer.write(value, sizeof(value)).is_ok());
    }

    EXPECT_EQ(state->output, "ok");
    EXPECT_EQ(state->flush_count, 0U);
}

}   // namespace
}   // namespace ca::io::test
