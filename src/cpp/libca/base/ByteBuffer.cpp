#include "ByteBuffer.hpp"

namespace ca {

ByteBuffer::ByteBuffer()
    : position_(0)
    , limit_(0)
    , capacity_(0)
    , buffer_(nullptr)
    , mark_(0)
{}

ByteBuffer::ByteBuffer(size_t size)
    : position_(0)
    , limit_(size)
    , capacity_(size)
    , buffer_(new uint8_t[size])
    , mark_(0)
{}

ByteBuffer::~ByteBuffer()
{
    delete[] buffer_;
}

size_t ByteBuffer::capacity() const
{
    return capacity_;
}

}   // namespace ca


#ifdef TEST_ENABLE

#    include "libca/test/Test.hpp"

using namespace ca::test;

TEST_CASE(ByteBuffer)
{
    ca::ByteBuffer buf = ca::ByteBuffer::allocate(4 * 1024);
    ASSERT_EQUAL(buf.capacity(), 4 * 1024);
}


#endif   // TEST_ENABLE
