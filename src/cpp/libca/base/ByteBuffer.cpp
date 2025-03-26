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

}   // namespace ca