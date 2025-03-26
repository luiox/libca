#ifndef LIBCA_BASE_BYTE_BUFFER_HPP
#define LIBCA_BASE_BYTE_BUFFER_HPP

#include <iostream>
#include <cstdint>
#include <cstring>

namespace ca {

class ByteBuffer
{
private:
    size_t   position_;
    size_t   limit_;
    size_t   capacity_;
    uint8_t* buffer_;
    size_t   mark_;

public:
    ByteBuffer();

    ByteBuffer(size_t size);

    ~ByteBuffer();

    static ByteBuffer allocate(size_t size) { return ByteBuffer(size); }

    // 包装一个现有的字节数组
    static ByteBuffer wrap(uint8_t* array, size_t size)
    {
        ByteBuffer bb;
        bb.buffer_   = array;
        bb.capacity_ = size;
        bb.limit_    = size;
        bb.position_ = 0;
        bb.mark_     = 0;
        return bb;
    }

    // 包装一个现有的字节数组的一部分
    static ByteBuffer wrap(uint8_t* array, size_t offset, size_t length)
    {
        ByteBuffer bb;
        bb.buffer_   = array + offset;
        bb.capacity_ = length;
        bb.limit_    = length;
        bb.position_ = 0;
        bb.mark_     = 0;
        return bb;
    }

    // // 获取当前位置
    // size_t position() const { return position_; }

    // // 设置当前位置
    // void position(size_t newPosition) { position_ = newPosition; }

    // // 获取限制
    // size_t limit() const { return limit_; }

    // // 设置限制
    // void limit(size_t newLimit) { limit_ = newLimit; }

    // // 获取容量
    // size_t capacity() const { return capacity_; }

    // // 标记当前位置
    // void mark() { mark_ = position_; }

    // // 重置到标记位置
    // void reset() { position_ = mark_; }

    // // 清除缓冲区
    // void clear()
    // {
    //     position_ = 0;
    //     limit_    = capacity_;
    //     mark_     = 0;
    // }

    // // 反转缓冲区
    // void flip()
    // {
    //     limit_    = position_;
    //     position_ = 0;
    // }

    // // 重绕缓冲区
    // void rewind() { position_ = 0; }

    // // 读取单个字节
    // uint8_t get() { return buffer_[position_++]; }

    // // 读取指定索引处的字节
    // uint8_t get(size_t index) { return buffer_[index]; }

    // // 将字节从此缓冲区传输到给定的目标数组
    // void get(uint8_t* dst, size_t offset, size_t length)
    // {
    //     std::memcpy(dst + offset, buffer_ + position_, length);
    //     position_ += length;
    // }

    // // 将单个字节写入缓冲区的当前位置
    // void put(uint8_t b) { buffer_[position_++] = b; }

    // // 将字节写入指定索引处的缓冲区
    // void put(size_t index, uint8_t b) { buffer_[index] = b; }

    // // 将给定数组中的字节序列写入此缓冲区的当前位置
    // void put(const uint8_t* src, size_t offset, size_t length)
    // {
    //     std::memcpy(buffer_ + position_, src + offset, length);
    //     position_ += length;
    // }
};

}   // namespace ca


#endif   // !LIBCA_BASE_BYTE_BUFFER_HPP