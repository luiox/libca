#include "libca/io/reader.hpp"

#include <algorithm>
#include <array>
#include <new>
#include <utility>

namespace ca::io {
namespace {

IoError invalid_buffer_error()
{
    return IoError::from_kind(IoErrorKind::InvalidInput,
                              "read buffer must not be null when length is nonzero");
}

IoError invalid_read_count_error()
{
    return IoError::from_kind(IoErrorKind::InvalidData,
                              "Reader returned more bytes than the requested capacity");
}

}   // namespace

IoResult<void> Reader::read_exact(u8* buffer, usize length)
{
    if (length != 0 && buffer == nullptr)
        return ca::core::Err(invalid_buffer_error());

    usize offset = 0;
    while (offset < length) {
        auto result = read(buffer + offset, length - offset);
        if (result.is_err()) {
            auto error = result.unwrap_err();
            if (error.kind() == IoErrorKind::Interrupted)
                continue;
            return ca::core::Err(std::move(error));
        }
        const usize count = result.unwrap();
        if (count > length - offset)
            return ca::core::Err(invalid_read_count_error());
        if (count == 0)
            return ca::core::Err(IoError::from_kind(
                IoErrorKind::UnexpectedEof, "stream ended before the requested bytes were read"));
        offset += count;
    }
    return ca::core::Ok();
}

IoResult<ca::core::Bytes> Reader::read_to_end(usize max_length)
{
    try {
        ca::core::BytesMut output = ca::core::BytesMut::with_capacity(
            std::min<usize>(max_length, static_cast<usize>(8192)));
        std::array<u8, 8192> buffer{};
        usize                total = 0;
        for (;;) {
            const usize remaining = max_length - total;
            const usize request =
                remaining < buffer.size() ? remaining + 1 : static_cast<usize>(buffer.size());
            auto result = read(buffer.data(), request);
            if (result.is_err()) {
                auto error = result.unwrap_err();
                if (error.kind() == IoErrorKind::Interrupted)
                    continue;
                return ca::core::Err(std::move(error));
            }
            const usize count = result.unwrap();
            if (count > request)
                return ca::core::Err(invalid_read_count_error());
            if (count == 0)
                return ca::core::Ok(output.freeze());
            if (count > remaining)
                return ca::core::Err(IoError::from_kind(
                    IoErrorKind::InvalidData, "stream exceeds the configured read limit"));
            output.put_slice(buffer.data(), count);
            total += count;
        }
    }
    catch (const std::bad_alloc& error) {
        return ca::core::Err(
            IoError::from_kind(IoErrorKind::OutOfMemory,
                               std::string("read buffer allocation failed: ") + error.what()));
    }
}

}   // namespace ca::io
