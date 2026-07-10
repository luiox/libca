#include "libca/io/seek.hpp"

namespace ca::io {

SeekFrom::SeekFrom(SeekOrigin origin, u64 absolute_position, i64 relative_offset) noexcept
    : origin_(origin)
    , absolute_position_(absolute_position)
    , relative_offset_(relative_offset)
{}

SeekFrom SeekFrom::start(u64 position) noexcept
{
    return SeekFrom(SeekOrigin::Start, position, 0);
}

SeekFrom SeekFrom::current(i64 offset) noexcept
{
    return SeekFrom(SeekOrigin::Current, 0, offset);
}

SeekFrom SeekFrom::end(i64 offset) noexcept
{
    return SeekFrom(SeekOrigin::End, 0, offset);
}

SeekOrigin SeekFrom::origin() const noexcept
{
    return origin_;
}

u64 SeekFrom::absolute_position() const noexcept
{
    return absolute_position_;
}

i64 SeekFrom::relative_offset() const noexcept
{
    return relative_offset_;
}

IoResult<u64> Seek::stream_position()
{
    return seek(SeekFrom::current(0));
}

IoResult<void> Seek::rewind()
{
    auto result = seek(SeekFrom::start(0));
    if (result.is_err())
        return ca::core::Err(result.unwrap_err());
    return ca::core::Ok();
}

}   // namespace ca::io
