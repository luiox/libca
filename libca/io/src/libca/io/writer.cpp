#include "libca/io/writer.hpp"

#include <utility>

namespace ca::io {

IoResult<void> Writer::write_all(const u8* data, usize length)
{
    if (length != 0 && data == nullptr)
        return ca::core::Err(IoError::from_kind(
            IoErrorKind::InvalidInput, "write data must not be null when length is nonzero"));

    usize offset = 0;
    while (offset < length) {
        auto result = write(data + offset, length - offset);
        if (result.is_err()) {
            auto error = result.unwrap_err();
            if (error.kind() == IoErrorKind::Interrupted)
                continue;
            return ca::core::Err(std::move(error));
        }
        const usize count = result.unwrap();
        if (count > length - offset)
            return ca::core::Err(IoError::from_kind(
                IoErrorKind::InvalidData, "Writer returned more bytes than the requested length"));
        if (count == 0)
            return ca::core::Err(IoError::from_kind(
                IoErrorKind::WriteZero, "Writer returned zero before all bytes were written"));
        offset += count;
    }
    return ca::core::Ok();
}

IoResult<void> Writer::write_all(ca::core::ByteSlice data)
{
    return write_all(data.data(), data.size());
}

}   // namespace ca::io
