#include "libca/io/buffered.hpp"

#include <algorithm>
#include <cstring>
#include <new>
#include <utility>

namespace ca::io {
namespace {

IoError invalid_buffered_stream(const char* operation)
{
    return IoError::from_kind(IoErrorKind::InvalidInput,
                              std::string(operation) + " on an empty buffered stream");
}

IoError invalid_read_count()
{
    return IoError::from_kind(IoErrorKind::InvalidData,
                              "Reader returned more bytes than the provided buffer");
}

IoError invalid_write_count()
{
    return IoError::from_kind(IoErrorKind::InvalidData,
                              "Writer returned more bytes than the provided input");
}

}   // namespace

BufReader::BufReader(std::unique_ptr<Reader> inner, std::vector<u8> buffer) noexcept
    : inner_(std::move(inner))
    , buffer_(std::move(buffer))
{}

BufReader::BufReader(BufReader&& other) noexcept
    : inner_(std::move(other.inner_))
    , buffer_(std::move(other.buffer_))
    , position_(other.position_)
    , filled_(other.filled_)
{
    other.position_ = 0;
    other.filled_   = 0;
}

BufReader& BufReader::operator=(BufReader&& other) noexcept
{
    if (this != &other) {
        inner_          = std::move(other.inner_);
        buffer_         = std::move(other.buffer_);
        position_       = other.position_;
        filled_         = other.filled_;
        other.position_ = 0;
        other.filled_   = 0;
    }
    return *this;
}

IoResult<BufReader> BufReader::create(std::unique_ptr<Reader> inner, usize capacity)
{
    if (inner == nullptr)
        return ca::core::Err(
            IoError::from_kind(IoErrorKind::InvalidInput, "BufReader requires a Reader"));
    if (capacity == 0)
        return ca::core::Err(
            IoError::from_kind(IoErrorKind::InvalidInput, "BufReader capacity must be nonzero"));
    try {
        return ca::core::Ok(BufReader(std::move(inner), std::vector<u8>(capacity)));
    }
    catch (const std::bad_alloc& error) {
        return ca::core::Err(
            IoError::from_kind(IoErrorKind::OutOfMemory,
                               std::string("BufReader buffer allocation failed: ") + error.what()));
    }
}

IoResult<usize> BufReader::read(u8* buffer, usize capacity)
{
    if (capacity == 0)
        return ca::core::Ok(static_cast<usize>(0));
    if (buffer == nullptr)
        return ca::core::Err(IoError::from_kind(
            IoErrorKind::InvalidInput, "read buffer must not be null when capacity is nonzero"));
    if (inner_ == nullptr)
        return ca::core::Err(invalid_buffered_stream("read"));

    const usize available = filled_ - position_;
    if (available != 0) {
        const usize count = std::min(available, capacity);
        std::memcpy(buffer, buffer_.data() + position_, count);
        position_ += count;
        return ca::core::Ok(count);
    }

    position_ = 0;
    filled_   = 0;
    if (capacity >= buffer_.size()) {
        auto result = inner_->read(buffer, capacity);
        if (result.is_ok() && result.unwrap() > capacity)
            return ca::core::Err(invalid_read_count());
        return result;
    }

    auto available_result = fill_buf();
    if (available_result.is_err())
        return ca::core::Err(available_result.unwrap_err());
    auto view = available_result.unwrap();
    if (view.empty())
        return ca::core::Ok(static_cast<usize>(0));
    const usize count = std::min(view.size(), capacity);
    std::memcpy(buffer, view.data(), count);
    position_ += count;
    return ca::core::Ok(count);
}

IoResult<ca::core::ByteSlice> BufReader::fill_buf()
{
    if (inner_ == nullptr)
        return ca::core::Err(invalid_buffered_stream("fill_buf"));
    if (position_ < filled_)
        return ca::core::Ok(ca::core::ByteSlice(buffer_.data() + position_, filled_ - position_));

    position_   = 0;
    filled_     = 0;
    auto result = inner_->read(buffer_.data(), buffer_.size());
    if (result.is_err())
        return ca::core::Err(result.unwrap_err());
    filled_ = result.unwrap();
    if (filled_ > buffer_.size()) {
        filled_ = 0;
        return ca::core::Err(invalid_read_count());
    }
    return ca::core::Ok(ca::core::ByteSlice(buffer_.data(), filled_));
}

IoResult<void> BufReader::consume(usize amount)
{
    if (inner_ == nullptr)
        return ca::core::Err(invalid_buffered_stream("consume"));
    if (amount > filled_ - position_)
        return ca::core::Err(
            IoError::from_kind(IoErrorKind::InvalidInput, "consume amount exceeds buffered data"));
    position_ += amount;
    return ca::core::Ok();
}

IoResult<usize> BufReader::read_until(u8 delimiter, ca::core::BytesMut& output)
{
    usize total = 0;
    try {
        for (;;) {
            auto available = fill_buf();
            if (available.is_err()) {
                auto error = available.unwrap_err();
                if (error.kind() == IoErrorKind::Interrupted)
                    continue;
                return ca::core::Err(std::move(error));
            }
            const auto view = available.unwrap();
            if (view.empty())
                return ca::core::Ok(total);

            const u8*   end   = view.data() + view.size();
            const u8*   found = std::find(view.data(), end, delimiter);
            const usize count =
                static_cast<usize>(found == end ? view.size() : found - view.data() + 1);
            output.put_slice(view.data(), count);
            position_ += count;
            total += count;
            if (found != end)
                return ca::core::Ok(total);
        }
    }
    catch (const std::bad_alloc& error) {
        return ca::core::Err(IoError::from_kind(
            IoErrorKind::OutOfMemory,
            std::string("read_until buffer allocation failed: ") + error.what()));
    }
}

usize BufReader::capacity() const noexcept
{
    return buffer_.size();
}

usize BufReader::buffered_len() const noexcept
{
    return filled_ - position_;
}

Reader* BufReader::inner() noexcept
{
    return inner_.get();
}

const Reader* BufReader::inner() const noexcept
{
    return inner_.get();
}

std::unique_ptr<Reader> BufReader::into_inner() noexcept
{
    position_ = 0;
    filled_   = 0;
    return std::move(inner_);
}

BufWriter::BufWriter(std::unique_ptr<Writer> inner, std::vector<u8> buffer) noexcept
    : inner_(std::move(inner))
    , buffer_(std::move(buffer))
{}

BufWriter::BufWriter(BufWriter&& other) noexcept
    : inner_(std::move(other.inner_))
    , buffer_(std::move(other.buffer_))
    , used_(other.used_)
{
    other.used_ = 0;
}

BufWriter& BufWriter::operator=(BufWriter&& other) noexcept
{
    if (this != &other) {
        if (inner_ != nullptr)
            flush_buffer();
        inner_      = std::move(other.inner_);
        buffer_     = std::move(other.buffer_);
        used_       = other.used_;
        other.used_ = 0;
    }
    return *this;
}

BufWriter::~BufWriter()
{
    if (inner_ != nullptr)
        flush_buffer();
}

IoResult<BufWriter> BufWriter::create(std::unique_ptr<Writer> inner, usize capacity)
{
    if (inner == nullptr)
        return ca::core::Err(
            IoError::from_kind(IoErrorKind::InvalidInput, "BufWriter requires a Writer"));
    if (capacity == 0)
        return ca::core::Err(
            IoError::from_kind(IoErrorKind::InvalidInput, "BufWriter capacity must be nonzero"));
    try {
        return ca::core::Ok(BufWriter(std::move(inner), std::vector<u8>(capacity)));
    }
    catch (const std::bad_alloc& error) {
        return ca::core::Err(
            IoError::from_kind(IoErrorKind::OutOfMemory,
                               std::string("BufWriter buffer allocation failed: ") + error.what()));
    }
}

IoResult<usize> BufWriter::write(const u8* data, usize length)
{
    if (length == 0)
        return ca::core::Ok(static_cast<usize>(0));
    if (data == nullptr)
        return ca::core::Err(IoError::from_kind(
            IoErrorKind::InvalidInput, "write data must not be null when length is nonzero"));
    if (inner_ == nullptr)
        return ca::core::Err(invalid_buffered_stream("write"));

    if (length > buffer_.size() - used_) {
        auto flushed = flush_buffer();
        if (flushed.is_err())
            return ca::core::Err(flushed.unwrap_err());
    }

    if (length >= buffer_.size()) {
        auto result = inner_->write(data, length);
        if (result.is_ok() && result.unwrap() > length)
            return ca::core::Err(invalid_write_count());
        return result;
    }

    std::memcpy(buffer_.data() + used_, data, length);
    used_ += length;
    return ca::core::Ok(length);
}

IoResult<void> BufWriter::flush()
{
    if (inner_ == nullptr)
        return ca::core::Err(invalid_buffered_stream("flush"));
    auto buffered = flush_buffer();
    if (buffered.is_err())
        return buffered;
    return inner_->flush();
}

IoResult<std::unique_ptr<Writer>> BufWriter::finish()
{
    auto result = flush();
    if (result.is_err())
        return ca::core::Err(result.unwrap_err());
    return ca::core::Ok(std::move(inner_));
}

usize BufWriter::capacity() const noexcept
{
    return buffer_.size();
}

usize BufWriter::buffered_len() const noexcept
{
    return used_;
}

Writer* BufWriter::inner() noexcept
{
    return inner_.get();
}

const Writer* BufWriter::inner() const noexcept
{
    return inner_.get();
}

IoResult<void> BufWriter::flush_buffer()
{
    if (inner_ == nullptr)
        return ca::core::Err(invalid_buffered_stream("flush_buffer"));

    usize written = 0;
    while (written < used_) {
        auto result = inner_->write(buffer_.data() + written, used_ - written);
        if (result.is_err()) {
            auto error = result.unwrap_err();
            if (error.kind() == IoErrorKind::Interrupted)
                continue;
            discard_written_prefix(written);
            return ca::core::Err(std::move(error));
        }
        const usize count = result.unwrap();
        if (count > used_ - written) {
            discard_written_prefix(written);
            return ca::core::Err(invalid_write_count());
        }
        if (count == 0) {
            discard_written_prefix(written);
            return ca::core::Err(IoError::from_kind(
                IoErrorKind::WriteZero, "Writer returned zero while flushing buffered data"));
        }
        written += count;
    }
    used_ = 0;
    return ca::core::Ok();
}

void BufWriter::discard_written_prefix(usize count) noexcept
{
    if (count == 0)
        return;
    const usize remaining = used_ - count;
    if (remaining != 0)
        std::memmove(buffer_.data(), buffer_.data() + count, remaining);
    used_ = remaining;
}

}   // namespace ca::io
