//
// @brief 简单可变的字节缓冲区 (ByteBuffer)
// @author Canrad
// @date 2026/05/31
// @note 基于 u8 类型存储，替代 std::vector<std::byte> / std::string 作为缓冲区
//       命名空间 ca::core，header-only
//

#ifndef LIBCA_CORE_BYTE_BUFFER_HPP
#define LIBCA_CORE_BYTE_BUFFER_HPP

#include <libca/core/datatype.hpp>

#include <cstring>
#include <stdexcept>
#include <utility>

namespace ca::core {

class ByteBuffer {
public:
    ByteBuffer() noexcept;
    explicit ByteBuffer(usize capacity);
    ByteBuffer(const u8* data, usize size);
    ByteBuffer(const ByteBuffer& other);
    ByteBuffer(ByteBuffer&& other) noexcept;
    ~ByteBuffer();

    ByteBuffer& operator=(const ByteBuffer& other);
    ByteBuffer& operator=(ByteBuffer&& other) noexcept;

    static ByteBuffer fromData(const u8* data, usize size);
    static ByteBuffer withCapacity(usize capacity);

    const u8* data() const noexcept;
    u8* data() noexcept;

    u8 operator[](usize index) const noexcept;
    u8& operator[](usize index) noexcept;
    u8 at(usize index) const;
    u8& at(usize index);
    u8 front() const;
    u8& front();
    u8 back() const;
    u8& back();

    usize size() const noexcept;
    usize capacity() const noexcept;
    bool empty() const noexcept;

    void reserve(usize newCapacity);
    void resize(usize newSize);
    void shrinkToFit();
    void clear() noexcept;

    void pushBack(u8 byte);
    void popBack();
    void append(const u8* data, usize size);
    void append(const ByteBuffer& other);
    void insert(usize pos, const u8* data, usize size);
    void erase(usize pos, usize count = 1);
    void assign(const u8* data, usize size);
    void swap(ByteBuffer& other) noexcept;

    bool equals(const ByteBuffer& other) const noexcept;
    bool operator==(const ByteBuffer& other) const noexcept;
    bool operator!=(const ByteBuffer& other) const noexcept;

private:
    u8*   data_;
    usize size_;
    usize capacity_;
    static constexpr usize kDefaultCapacity = 32;
    void grow(usize minCapacity);
};


// ============================================================================
// 内联实现
// ============================================================================

inline ByteBuffer::ByteBuffer() noexcept
    : data_(nullptr), size_(0), capacity_(0) {}

inline ByteBuffer::ByteBuffer(usize capacity)
    : data_(new u8[capacity > 0 ? capacity : 1]), size_(0), capacity_(capacity > 0 ? capacity : 1) {}

inline ByteBuffer::ByteBuffer(const u8* data, usize size)
    : data_(new u8[size > 0 ? size : kDefaultCapacity])
    , size_(size)
    , capacity_(size > 0 ? size : kDefaultCapacity) {
    if (size > 0) std::memcpy(data_, data, size);
}

inline ByteBuffer::ByteBuffer(const ByteBuffer& other)
    : data_(new u8[other.capacity_]), size_(other.size_), capacity_(other.capacity_) {
    if (size_ > 0) std::memcpy(data_, other.data_, size_);
}

inline ByteBuffer::ByteBuffer(ByteBuffer&& other) noexcept
    : data_(other.data_), size_(other.size_), capacity_(other.capacity_) {
    other.data_ = nullptr; other.size_ = 0; other.capacity_ = 0;
}

inline ByteBuffer::~ByteBuffer() { delete[] data_; }

inline ByteBuffer& ByteBuffer::operator=(const ByteBuffer& other) {
    if (this != &other) {
        auto* newBuf = new u8[other.capacity_];
        if (other.size_ > 0) std::memcpy(newBuf, other.data_, other.size_);
        delete[] data_;
        data_ = newBuf;
        size_ = other.size_;
        capacity_ = other.capacity_;
    }
    return *this;
}

inline ByteBuffer& ByteBuffer::operator=(ByteBuffer&& other) noexcept {
    if (this != &other) {
        delete[] data_;
        data_ = other.data_; size_ = other.size_; capacity_ = other.capacity_;
        other.data_ = nullptr; other.size_ = 0; other.capacity_ = 0;
    }
    return *this;
}

inline ByteBuffer ByteBuffer::fromData(const u8* data, usize size) { return ByteBuffer(data, size); }
inline ByteBuffer ByteBuffer::withCapacity(usize capacity) { return ByteBuffer(capacity); }

inline const u8* ByteBuffer::data() const noexcept { return data_; }
inline u8* ByteBuffer::data() noexcept { return data_; }
inline u8 ByteBuffer::operator[](usize index) const noexcept { return data_[index]; }
inline u8& ByteBuffer::operator[](usize index) noexcept { return data_[index]; }

inline u8 ByteBuffer::at(usize index) const {
    if (index >= size_) throw std::out_of_range("ByteBuffer::at");
    return data_[index];
}

inline u8& ByteBuffer::at(usize index) {
    if (index >= size_) throw std::out_of_range("ByteBuffer::at");
    return data_[index];
}

inline u8 ByteBuffer::front() const { return data_[0]; }
inline u8& ByteBuffer::front() { return data_[0]; }
inline u8 ByteBuffer::back() const { return data_[size_ - 1]; }
inline u8& ByteBuffer::back() { return data_[size_ - 1]; }

inline usize ByteBuffer::size() const noexcept { return size_; }
inline usize ByteBuffer::capacity() const noexcept { return capacity_; }
inline bool ByteBuffer::empty() const noexcept { return size_ == 0; }

inline void ByteBuffer::reserve(usize newCapacity) {
    if (newCapacity > capacity_) {
        auto newBuf = new u8[newCapacity];
        if (size_ > 0) std::memcpy(newBuf, data_, size_);
        delete[] data_;
        data_ = newBuf; capacity_ = newCapacity;
    }
}

inline void ByteBuffer::resize(usize newSize) {
    if (newSize > capacity_) {
        usize newCap = capacity_ * 2;
        if (newCap < newSize) newCap = newSize;
        reserve(newCap);
    }
    if (newSize > size_) std::memset(data_ + size_, 0, newSize - size_);
    size_ = newSize;
}

inline void ByteBuffer::shrinkToFit() {
    if (size_ < capacity_) {
        auto newBuf = new u8[size_ > 0 ? size_ : 1];
        if (size_ > 0) std::memcpy(newBuf, data_, size_);
        delete[] data_;
        data_ = newBuf; capacity_ = size_ > 0 ? size_ : 1;
    }
}

inline void ByteBuffer::clear() noexcept { size_ = 0; }

inline void ByteBuffer::pushBack(u8 byte) {
    if (size_ >= capacity_) grow(size_ + 1);
    data_[size_++] = byte;
}

inline void ByteBuffer::popBack() { if (size_ > 0) --size_; }

inline void ByteBuffer::append(const u8* data, usize len) {
    if (len == 0) return;
    auto needed = size_ + len;
    if (needed > capacity_) grow(needed);
    std::memcpy(data_ + size_, data, len);
    size_ = needed;
}

inline void ByteBuffer::append(const ByteBuffer& other) { append(other.data_, other.size_); }

inline void ByteBuffer::insert(usize pos, const u8* data, usize len) {
    if (pos > size_ || len == 0) return;
    auto needed = size_ + len;
    if (needed > capacity_) grow(needed);
    std::memmove(data_ + pos + len, data_ + pos, size_ - pos);
    std::memcpy(data_ + pos, data, len);
    size_ = needed;
}

inline void ByteBuffer::erase(usize pos, usize count) {
    if (pos >= size_ || count == 0) return;
    if (pos + count > size_) count = size_ - pos;
    std::memmove(data_ + pos, data_ + pos + count, size_ - pos - count);
    size_ -= count;
}

inline void ByteBuffer::assign(const u8* data, usize size) {
    if (size > capacity_) reserve(size);
    std::memcpy(data_, data, size);
    size_ = size;
}

inline void ByteBuffer::swap(ByteBuffer& other) noexcept {
    using std::swap;
    swap(data_, other.data_);
    swap(size_, other.size_);
    swap(capacity_, other.capacity_);
}

inline bool ByteBuffer::equals(const ByteBuffer& other) const noexcept {
    if (size_ != other.size_) return false;
    return size_ == 0 || std::memcmp(data_, other.data_, size_) == 0;
}

inline bool ByteBuffer::operator==(const ByteBuffer& other) const noexcept { return equals(other); }
inline bool ByteBuffer::operator!=(const ByteBuffer& other) const noexcept { return !equals(other); }

inline void ByteBuffer::grow(usize minCapacity) {
    usize newCap = capacity_ * 2;
    if (newCap < minCapacity) newCap = minCapacity;
    if (newCap < kDefaultCapacity) newCap = kDefaultCapacity;
    auto newBuf = new u8[newCap];
    if (size_ > 0) std::memcpy(newBuf, data_, size_);
    delete[] data_;
    data_ = newBuf; capacity_ = newCap;
}

}  // namespace ca::core

#endif  // LIBCA_CORE_BYTE_BUFFER_HPP
