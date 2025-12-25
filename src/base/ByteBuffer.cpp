#include "ByteBuffer.hpp"

namespace ca {

ByteBuffer::ByteBuffer(size_t size)
    : position_(0),
      limit_(size),
      capacity_(size),
      owned_(new uint8_t[size]()),
      buffer_(owned_.get()),
      owns_(true),
      mark_(std::numeric_limits<size_t>::max())
{
}

void ByteBuffer::ensureRemaining(size_t n) const
{
    if (position_ + n > limit_) {
        throw std::out_of_range("ByteBuffer: not enough remaining bytes");
    }
}

void ByteBuffer::checkIndex(size_t index) const
{
    if (index >= limit_) {
        throw std::out_of_range("ByteBuffer: index out of range");
    }
}

ByteBuffer ByteBuffer::wrap(uint8_t* array, size_t size)
{
    ByteBuffer bb;
    bb.buffer_   = array;
    bb.capacity_ = size;
    bb.limit_    = size;
    bb.position_ = 0;
    bb.owns_     = false;
    bb.mark_     = std::numeric_limits<size_t>::max();
    return bb;
}

ByteBuffer ByteBuffer::wrap(uint8_t* array, size_t offset, size_t length)
{
    ByteBuffer bb;
    bb.buffer_   = array + offset;
    bb.capacity_ = length;
    bb.limit_    = length;
    bb.position_ = 0;
    bb.owns_     = false;
    bb.mark_     = std::numeric_limits<size_t>::max();
    return bb;
}

size_t ByteBuffer::capacity() const { return capacity_; }
size_t ByteBuffer::position() const { return position_; }
void ByteBuffer::position(size_t newPosition)
{
    if (newPosition > limit_) throw std::out_of_range("ByteBuffer: position > limit");
    position_ = newPosition;
}

size_t ByteBuffer::limit() const { return limit_; }
void ByteBuffer::limit(size_t newLimit)
{
    if (newLimit > capacity_) throw std::out_of_range("ByteBuffer: limit > capacity");
    limit_ = newLimit;
    if (position_ > limit_) position_ = limit_;
}

size_t ByteBuffer::remaining() const { return (limit_ >= position_) ? (limit_ - position_) : 0; }
bool ByteBuffer::hasRemaining() const { return remaining() > 0; }

void ByteBuffer::mark() { mark_ = position_; }
void ByteBuffer::reset()
{
    if (mark_ == std::numeric_limits<size_t>::max())
        throw std::runtime_error("ByteBuffer: mark not set");
    position_ = mark_;
}

void ByteBuffer::clear()
{
    position_ = 0;
    limit_    = capacity_;
    mark_     = std::numeric_limits<size_t>::max();
}

void ByteBuffer::flip()
{
    limit_    = position_;
    position_ = 0;
    mark_     = std::numeric_limits<size_t>::max();
}

void ByteBuffer::rewind() { position_ = 0; mark_ = std::numeric_limits<size_t>::max(); }

uint8_t ByteBuffer::get()
{
    ensureRemaining(1);
    return buffer_[position_++];
}

uint8_t ByteBuffer::get(size_t index) const
{
    checkIndex(index);
    return buffer_[index];
}

void ByteBuffer::get(uint8_t* dst, size_t offset, size_t length)
{
    ensureRemaining(length);
    std::memcpy(dst + offset, buffer_ + position_, length);
    position_ += length;
}

void ByteBuffer::put(uint8_t b)
{
    ensureRemaining(1);
    buffer_[position_++] = b;
}

void ByteBuffer::put(size_t index, uint8_t b)
{
    if (index >= limit_) throw std::out_of_range("ByteBuffer: put index out of range");
    buffer_[index] = b;
}

void ByteBuffer::put(const uint8_t* src, size_t offset, size_t length)
{
    ensureRemaining(length);
    std::memcpy(buffer_ + position_, src + offset, length);
    position_ += length;
}

ByteBuffer ByteBuffer::slice() const
{
    ByteBuffer bb;
    bb.buffer_   = buffer_ + position_;
    bb.capacity_ = (limit_ >= position_) ? (limit_ - position_) : 0;
    bb.limit_    = bb.capacity_;
    bb.position_ = 0;
    bb.owns_     = false;
    bb.mark_     = std::numeric_limits<size_t>::max();
    return bb;
}

void ByteBuffer::compact()
{
    size_t rem = remaining();
    if (rem && position_ > 0) {
        std::memmove(buffer_, buffer_ + position_, rem);
    }
    position_ = rem;
    limit_ = capacity_;
    mark_ = std::numeric_limits<size_t>::max();
}

uint8_t* ByteBuffer::data() noexcept { return buffer_; }
const uint8_t* ByteBuffer::data() const noexcept { return buffer_; }

}   // namespace ca


#if TEST_ENABLE

#    include "../test/Test.hpp"

using namespace ca;
using namespace ca::test;

TEST_CASE("ByteBuffer basic operations")
{
    // allocate, put, flip, get
    ByteBuffer b = ByteBuffer::allocate(5);
    ASSERT_EQUAL(5, b.capacity());
    b.put(0x11);
    b.put(0x22);
    ASSERT_EQUAL(2, b.position());
    b.flip();
    ASSERT_EQUAL(0, b.position());
    ASSERT_EQUAL(2, b.limit());
    ASSERT_TRUE(b.hasRemaining());
    ASSERT_EQUAL(0x11, (int)b.get());
    ASSERT_EQUAL(1, b.position());
    ASSERT_EQUAL(0x22, (int)b.get());
    ASSERT_FALSE(b.hasRemaining());

    // wrap external buffer and index put
    uint8_t arr[4] = {1, 2, 3, 4};
    ByteBuffer wb = ByteBuffer::wrap(arr, 4);
    ASSERT_EQUAL(4, wb.capacity());
    ASSERT_EQUAL(1, (int)wb.get());
    wb.put(0, 9);
    ASSERT_EQUAL(9, (int)arr[0]);

    // put/get with memcpy, slice and compact
    ByteBuffer b2 = ByteBuffer::allocate(6);
    const uint8_t src[4] = {11, 12, 13, 14};
    b2.put(src, 0, 4);
    b2.flip();
    uint8_t dst[4] = {0};
    b2.get(dst, 0, 4);
    ASSERT_EQUAL(11, (int)dst[0]);
    // refill and test slice/compact
    b2.clear();
    b2.put(1); b2.put(2); b2.put(3); b2.put(4);
    b2.flip(); // limit = 4, position = 0
    b2.get(); b2.get(); // pos = 2
    ByteBuffer s = b2.slice();
    ASSERT_EQUAL(2, s.capacity());
    ASSERT_EQUAL(3, (int)s.get()); // slice starts from original position
    b2.compact();
    ASSERT_EQUAL(2, b2.position());
    ASSERT_EQUAL(b2.capacity(), b2.limit());

    // mark/reset
    ByteBuffer b3 = ByteBuffer::allocate(3);
    b3.put(10); b3.put(20);
    b3.flip();
    ASSERT_EQUAL(10, (int)b3.get());
    b3.mark();
    ASSERT_EQUAL(20, (int)b3.get());
    b3.reset();
    ASSERT_EQUAL(20, (int)b3.get());

    // move semantics
    ByteBuffer b4 = ByteBuffer::allocate(4);
    b4.put(7); b4.put(8);
    b4.flip();
    ByteBuffer b5 = std::move(b4);
    ASSERT_EQUAL(7, (int)b5.get());
    ASSERT_EQUAL(8, (int)b5.get());
}


#endif   // TEST_ENABLE
