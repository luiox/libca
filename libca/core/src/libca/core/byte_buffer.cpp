//
// @brief ByteBuffer 非内联实现
// @author Canrad
// @date 2026/05/31
//

#include <libca/core/byte_buffer.hpp>

#include <algorithm>
#include <cstring>
#include <stdexcept>

namespace ca::core {

// ============================================================================
// 工厂方法
// ============================================================================

ByteBuffer ByteBuffer::allocate(usize capacity) {
    return ByteBuffer(capacity);
}

ByteBuffer ByteBuffer::wrap(u8* data, usize size) {
    ByteBuffer buf;
    buf.data_ = data;
    buf.capacity_ = size;
    buf.limit_ = size;
    buf.position_ = 0;
    buf.owns_ = false;
    buf.markValid_ = false;
    return buf;
}

ByteBuffer ByteBuffer::copyOf(const u8* data, usize size) {
    return ByteBuffer(data, size);
}

// ============================================================================
// 构造 / 析构 / 赋值
// ============================================================================

ByteBuffer::ByteBuffer() noexcept
    : data_(nullptr), capacity_(0), position_(0), limit_(0), mark_(0),
      order_(ByteOrder::BigEndian), owns_(true), markValid_(false) {}

ByteBuffer::ByteBuffer(usize capacity)
    : data_(new u8[capacity > 0 ? capacity : 1]),
      capacity_(capacity > 0 ? capacity : 1),
      position_(0), limit_(capacity),
      mark_(0), order_(ByteOrder::BigEndian),
      owns_(true), markValid_(false) {}

ByteBuffer::ByteBuffer(const u8* data, usize size)
    : data_(new u8[size > 0 ? size : kDefaultCapacity]),
      capacity_(size > 0 ? size : kDefaultCapacity),
      position_(0), limit_(size),
      mark_(0), order_(ByteOrder::BigEndian),
      owns_(true), markValid_(false) {
    if (size > 0) {
        std::memcpy(data_, data, size);
    }
}

ByteBuffer::ByteBuffer(const ByteBuffer& other)
    : data_(new u8[other.capacity_]),
      capacity_(other.capacity_),
      position_(other.position_),
      limit_(other.limit_),
      mark_(other.mark_),
      order_(other.order_),
      owns_(true),
      markValid_(other.markValid_) {
    if (limit_ > 0) {
        std::memcpy(data_, other.data_, limit_);
    }
}

ByteBuffer::ByteBuffer(ByteBuffer&& other) noexcept
    : data_(other.data_),
      capacity_(other.capacity_),
      position_(other.position_),
      limit_(other.limit_),
      mark_(other.mark_),
      order_(other.order_),
      owns_(other.owns_),
      markValid_(other.markValid_) {
    other.data_ = nullptr;
    other.capacity_ = 0;
    other.position_ = 0;
    other.limit_ = 0;
    other.owns_ = false;
    other.markValid_ = false;
}

ByteBuffer::~ByteBuffer() {
    if (owns_) {
        delete[] data_;
    }
}

ByteBuffer& ByteBuffer::operator=(const ByteBuffer& other) {
    if (this != &other) {
        auto* newBuf = new u8[other.capacity_];
        if (other.limit_ > 0) {
            std::memcpy(newBuf, other.data_, other.limit_);
        }
        if (owns_) {
            delete[] data_;
        }
        data_       = newBuf;
        capacity_   = other.capacity_;
        position_   = other.position_;
        limit_      = other.limit_;
        mark_       = other.mark_;
        order_      = other.order_;
        owns_       = true;
        markValid_  = other.markValid_;
    }
    return *this;
}

ByteBuffer& ByteBuffer::operator=(ByteBuffer&& other) noexcept {
    if (this != &other) {
        if (owns_) {
            delete[] data_;
        }
        data_       = other.data_;
        capacity_   = other.capacity_;
        position_   = other.position_;
        limit_      = other.limit_;
        mark_       = other.mark_;
        order_      = other.order_;
        owns_       = other.owns_;
        markValid_  = other.markValid_;

        other.data_ = nullptr;
        other.capacity_ = 0;
        other.position_ = 0;
        other.limit_ = 0;
        other.owns_ = false;
        other.markValid_ = false;
    }
    return *this;
}

// ============================================================================
// 游标控制
// ============================================================================

void ByteBuffer::position(usize newPosition) {
    if (newPosition > limit_) {
        throw std::out_of_range("ByteBuffer::position out of range");
    }
    position_ = newPosition;
}

void ByteBuffer::limit(usize newLimit) {
    if (newLimit > capacity_) {
        throw std::out_of_range("ByteBuffer::limit out of range");
    }
    limit_ = newLimit;
    if (position_ > limit_) {
        position_ = limit_;
    }
}

void ByteBuffer::flip() {
    limit_ = position_;
    position_ = 0;
    markValid_ = false;
}

void ByteBuffer::rewind() {
    position_ = 0;
    markValid_ = false;
}

void ByteBuffer::clear() noexcept {
    position_ = 0;
    limit_ = capacity_;
    markValid_ = false;
}

void ByteBuffer::compact() {
    usize unread = remaining();
    if (position_ > 0 && unread > 0) {
        std::memmove(data_, data_ + position_, unread);
    }
    position_ = unread;
    limit_ = capacity_;
    markValid_ = false;
}

void ByteBuffer::mark() {
    mark_ = position_;
    markValid_ = true;
}

void ByteBuffer::reset() {
    if (!markValid_) {
        throw std::runtime_error("ByteBuffer::reset with no mark");
    }
    position_ = mark_;
}

// ============================================================================
// 相对读写
// ============================================================================

u8 ByteBuffer::get() {
    if (position_ >= limit_) {
        throw std::out_of_range("ByteBuffer::get underflow");
    }
    return data_[position_++];
}

void ByteBuffer::put(u8 b) {
    ensureWritable(1);
    data_[position_++] = b;
}

void ByteBuffer::get(u8* dst, usize length) {
    if (position_ + length > limit_) {
        throw std::out_of_range("ByteBuffer::get(bulk) underflow");
    }
    std::memcpy(dst, data_ + position_, length);
    position_ += length;
}

void ByteBuffer::put(const u8* src, usize length) {
    if (length == 0) return;
    ensureWritable(length);
    std::memcpy(data_ + position_, src, length);
    position_ += length;
}

void ByteBuffer::put(const ByteBuffer& src) {
    usize count = src.remaining();
    if (count == 0) return;
    ensureWritable(count);
    std::memcpy(data_ + position_, src.data_ + src.position_, count);
    position_ += count;
}

// ============================================================================
// 绝对读写
// ============================================================================

u8 ByteBuffer::get(usize index) const {
    if (index >= limit_) {
        throw std::out_of_range("ByteBuffer::get(index) out of range");
    }
    return data_[index];
}

void ByteBuffer::put(usize index, u8 b) {
    if (index >= limit_) {
        throw std::out_of_range("ByteBuffer::put(index) out of range");
    }
    data_[index] = b;
}

void ByteBuffer::get(usize index, u8* dst, usize length) const {
    if (index + length > limit_) {
        throw std::out_of_range("ByteBuffer::get(index,dst,len) out of range");
    }
    std::memcpy(dst, data_ + index, length);
}

void ByteBuffer::put(usize index, const u8* src, usize length) {
    if (index + length > limit_) {
        throw std::out_of_range("ByteBuffer::put(index,src,len) out of range");
    }
    std::memcpy(data_ + index, src, length);
}

// ============================================================================
// at() 边界检查访问
// ============================================================================

u8 ByteBuffer::at(usize index) const {
    if (index >= limit_) {
        throw std::out_of_range("ByteBuffer::at");
    }
    return data_[index];
}

u8& ByteBuffer::at(usize index) {
    if (index >= limit_) {
        throw std::out_of_range("ByteBuffer::at");
    }
    return data_[index];
}

// ============================================================================
// 类型化读写 — 相对
// ============================================================================

u16 ByteBuffer::getU16() {
    if (position_ + 2 > limit_) {
        throw std::out_of_range("ByteBuffer::getU16 underflow");
    }
    u16 val;
    if (order_ == ByteOrder::BigEndian) {
        val = (static_cast<u16>(data_[position_])     << 8) |
               static_cast<u16>(data_[position_ + 1]);
    } else {
        val = (static_cast<u16>(data_[position_ + 1]) << 8) |
               static_cast<u16>(data_[position_]);
    }
    position_ += 2;
    return val;
}

void ByteBuffer::putU16(u16 value) {
    ensureWritable(2);
    if (order_ == ByteOrder::BigEndian) {
        data_[position_]     = static_cast<u8>((value >> 8) & 0xFF);
        data_[position_ + 1] = static_cast<u8>(value & 0xFF);
    } else {
        data_[position_]     = static_cast<u8>(value & 0xFF);
        data_[position_ + 1] = static_cast<u8>((value >> 8) & 0xFF);
    }
    position_ += 2;
}

u32 ByteBuffer::getU32() {
    if (position_ + 4 > limit_) {
        throw std::out_of_range("ByteBuffer::getU32 underflow");
    }
    u32 val;
    if (order_ == ByteOrder::BigEndian) {
        val = (static_cast<u32>(data_[position_])     << 24) |
              (static_cast<u32>(data_[position_ + 1]) << 16) |
              (static_cast<u32>(data_[position_ + 2]) <<  8) |
               static_cast<u32>(data_[position_ + 3]);
    } else {
        val = (static_cast<u32>(data_[position_ + 3]) << 24) |
              (static_cast<u32>(data_[position_ + 2]) << 16) |
              (static_cast<u32>(data_[position_ + 1]) <<  8) |
               static_cast<u32>(data_[position_]);
    }
    position_ += 4;
    return val;
}

void ByteBuffer::putU32(u32 value) {
    ensureWritable(4);
    if (order_ == ByteOrder::BigEndian) {
        data_[position_]     = static_cast<u8>((value >> 24) & 0xFF);
        data_[position_ + 1] = static_cast<u8>((value >> 16) & 0xFF);
        data_[position_ + 2] = static_cast<u8>((value >>  8) & 0xFF);
        data_[position_ + 3] = static_cast<u8>(value & 0xFF);
    } else {
        data_[position_]     = static_cast<u8>(value & 0xFF);
        data_[position_ + 1] = static_cast<u8>((value >>  8) & 0xFF);
        data_[position_ + 2] = static_cast<u8>((value >> 16) & 0xFF);
        data_[position_ + 3] = static_cast<u8>((value >> 24) & 0xFF);
    }
    position_ += 4;
}

u64 ByteBuffer::getU64() {
    if (position_ + 8 > limit_) {
        throw std::out_of_range("ByteBuffer::getU64 underflow");
    }
    u64 val;
    if (order_ == ByteOrder::BigEndian) {
        val = (static_cast<u64>(data_[position_])     << 56) |
              (static_cast<u64>(data_[position_ + 1]) << 48) |
              (static_cast<u64>(data_[position_ + 2]) << 40) |
              (static_cast<u64>(data_[position_ + 3]) << 32) |
              (static_cast<u64>(data_[position_ + 4]) << 24) |
              (static_cast<u64>(data_[position_ + 5]) << 16) |
              (static_cast<u64>(data_[position_ + 6]) <<  8) |
               static_cast<u64>(data_[position_ + 7]);
    } else {
        val = (static_cast<u64>(data_[position_ + 7]) << 56) |
              (static_cast<u64>(data_[position_ + 6]) << 48) |
              (static_cast<u64>(data_[position_ + 5]) << 40) |
              (static_cast<u64>(data_[position_ + 4]) << 32) |
              (static_cast<u64>(data_[position_ + 3]) << 24) |
              (static_cast<u64>(data_[position_ + 2]) << 16) |
              (static_cast<u64>(data_[position_ + 1]) <<  8) |
               static_cast<u64>(data_[position_]);
    }
    position_ += 8;
    return val;
}

void ByteBuffer::putU64(u64 value) {
    ensureWritable(8);
    if (order_ == ByteOrder::BigEndian) {
        data_[position_]     = static_cast<u8>((value >> 56) & 0xFF);
        data_[position_ + 1] = static_cast<u8>((value >> 48) & 0xFF);
        data_[position_ + 2] = static_cast<u8>((value >> 40) & 0xFF);
        data_[position_ + 3] = static_cast<u8>((value >> 32) & 0xFF);
        data_[position_ + 4] = static_cast<u8>((value >> 24) & 0xFF);
        data_[position_ + 5] = static_cast<u8>((value >> 16) & 0xFF);
        data_[position_ + 6] = static_cast<u8>((value >>  8) & 0xFF);
        data_[position_ + 7] = static_cast<u8>(value & 0xFF);
    } else {
        data_[position_]     = static_cast<u8>(value & 0xFF);
        data_[position_ + 1] = static_cast<u8>((value >>  8) & 0xFF);
        data_[position_ + 2] = static_cast<u8>((value >> 16) & 0xFF);
        data_[position_ + 3] = static_cast<u8>((value >> 24) & 0xFF);
        data_[position_ + 4] = static_cast<u8>((value >> 32) & 0xFF);
        data_[position_ + 5] = static_cast<u8>((value >> 40) & 0xFF);
        data_[position_ + 6] = static_cast<u8>((value >> 48) & 0xFF);
        data_[position_ + 7] = static_cast<u8>((value >> 56) & 0xFF);
    }
    position_ += 8;
}

// ── 有符号整数：委托无符号 + 转型 ──

i16 ByteBuffer::getI16() { return static_cast<i16>(getU16()); }
void ByteBuffer::putI16(i16 value) { putU16(static_cast<u16>(value)); }
i32 ByteBuffer::getI32() { return static_cast<i32>(getU32()); }
void ByteBuffer::putI32(i32 value) { putU32(static_cast<u32>(value)); }
i64 ByteBuffer::getI64() { return static_cast<i64>(getU64()); }
void ByteBuffer::putI64(i64 value) { putU64(static_cast<u64>(value)); }

// ── 浮点数：bit-cast 委托 ──

f32 ByteBuffer::getF32() {
    u32 bits = getU32();
    f32 val;
    std::memcpy(&val, &bits, sizeof(f32));
    return val;
}

void ByteBuffer::putF32(f32 value) {
    u32 bits;
    std::memcpy(&bits, &value, sizeof(f32));
    putU32(bits);
}

f64 ByteBuffer::getF64() {
    u64 bits = getU64();
    f64 val;
    std::memcpy(&val, &bits, sizeof(f64));
    return val;
}

void ByteBuffer::putF64(f64 value) {
    u64 bits;
    std::memcpy(&bits, &value, sizeof(f64));
    putU64(bits);
}

// ============================================================================
// 类型化读写 — 绝对
// ============================================================================

u16 ByteBuffer::getU16(usize index) const {
    if (index + 2 > limit_) {
        throw std::out_of_range("ByteBuffer::getU16(index) out of range");
    }
    if (order_ == ByteOrder::BigEndian) {
        return (static_cast<u16>(data_[index])     << 8) |
                static_cast<u16>(data_[index + 1]);
    } else {
        return (static_cast<u16>(data_[index + 1]) << 8) |
                static_cast<u16>(data_[index]);
    }
}

void ByteBuffer::putU16(usize index, u16 value) {
    if (index + 2 > limit_) {
        throw std::out_of_range("ByteBuffer::putU16(index) out of range");
    }
    if (order_ == ByteOrder::BigEndian) {
        data_[index]     = static_cast<u8>((value >> 8) & 0xFF);
        data_[index + 1] = static_cast<u8>(value & 0xFF);
    } else {
        data_[index]     = static_cast<u8>(value & 0xFF);
        data_[index + 1] = static_cast<u8>((value >> 8) & 0xFF);
    }
}

u32 ByteBuffer::getU32(usize index) const {
    if (index + 4 > limit_) {
        throw std::out_of_range("ByteBuffer::getU32(index) out of range");
    }
    if (order_ == ByteOrder::BigEndian) {
        return (static_cast<u32>(data_[index])     << 24) |
               (static_cast<u32>(data_[index + 1]) << 16) |
               (static_cast<u32>(data_[index + 2]) <<  8) |
                static_cast<u32>(data_[index + 3]);
    } else {
        return (static_cast<u32>(data_[index + 3]) << 24) |
               (static_cast<u32>(data_[index + 2]) << 16) |
               (static_cast<u32>(data_[index + 1]) <<  8) |
                static_cast<u32>(data_[index]);
    }
}

void ByteBuffer::putU32(usize index, u32 value) {
    if (index + 4 > limit_) {
        throw std::out_of_range("ByteBuffer::putU32(index) out of range");
    }
    if (order_ == ByteOrder::BigEndian) {
        data_[index]     = static_cast<u8>((value >> 24) & 0xFF);
        data_[index + 1] = static_cast<u8>((value >> 16) & 0xFF);
        data_[index + 2] = static_cast<u8>((value >>  8) & 0xFF);
        data_[index + 3] = static_cast<u8>(value & 0xFF);
    } else {
        data_[index]     = static_cast<u8>(value & 0xFF);
        data_[index + 1] = static_cast<u8>((value >>  8) & 0xFF);
        data_[index + 2] = static_cast<u8>((value >> 16) & 0xFF);
        data_[index + 3] = static_cast<u8>((value >> 24) & 0xFF);
    }
}

u64 ByteBuffer::getU64(usize index) const {
    if (index + 8 > limit_) {
        throw std::out_of_range("ByteBuffer::getU64(index) out of range");
    }
    if (order_ == ByteOrder::BigEndian) {
        return (static_cast<u64>(data_[index])     << 56) |
               (static_cast<u64>(data_[index + 1]) << 48) |
               (static_cast<u64>(data_[index + 2]) << 40) |
               (static_cast<u64>(data_[index + 3]) << 32) |
               (static_cast<u64>(data_[index + 4]) << 24) |
               (static_cast<u64>(data_[index + 5]) << 16) |
               (static_cast<u64>(data_[index + 6]) <<  8) |
                static_cast<u64>(data_[index + 7]);
    } else {
        return (static_cast<u64>(data_[index + 7]) << 56) |
               (static_cast<u64>(data_[index + 6]) << 48) |
               (static_cast<u64>(data_[index + 5]) << 40) |
               (static_cast<u64>(data_[index + 4]) << 32) |
               (static_cast<u64>(data_[index + 3]) << 24) |
               (static_cast<u64>(data_[index + 2]) << 16) |
               (static_cast<u64>(data_[index + 1]) <<  8) |
                static_cast<u64>(data_[index]);
    }
}

void ByteBuffer::putU64(usize index, u64 value) {
    if (index + 8 > limit_) {
        throw std::out_of_range("ByteBuffer::putU64(index) out of range");
    }
    if (order_ == ByteOrder::BigEndian) {
        data_[index]     = static_cast<u8>((value >> 56) & 0xFF);
        data_[index + 1] = static_cast<u8>((value >> 48) & 0xFF);
        data_[index + 2] = static_cast<u8>((value >> 40) & 0xFF);
        data_[index + 3] = static_cast<u8>((value >> 32) & 0xFF);
        data_[index + 4] = static_cast<u8>((value >> 24) & 0xFF);
        data_[index + 5] = static_cast<u8>((value >> 16) & 0xFF);
        data_[index + 6] = static_cast<u8>((value >>  8) & 0xFF);
        data_[index + 7] = static_cast<u8>(value & 0xFF);
    } else {
        data_[index]     = static_cast<u8>(value & 0xFF);
        data_[index + 1] = static_cast<u8>((value >>  8) & 0xFF);
        data_[index + 2] = static_cast<u8>((value >> 16) & 0xFF);
        data_[index + 3] = static_cast<u8>((value >> 24) & 0xFF);
        data_[index + 4] = static_cast<u8>((value >> 32) & 0xFF);
        data_[index + 5] = static_cast<u8>((value >> 40) & 0xFF);
        data_[index + 6] = static_cast<u8>((value >> 48) & 0xFF);
        data_[index + 7] = static_cast<u8>((value >> 56) & 0xFF);
    }
}

i16 ByteBuffer::getI16(usize index) const { return static_cast<i16>(getU16(index)); }
void ByteBuffer::putI16(usize index, i16 value) { putU16(index, static_cast<u16>(value)); }
i32 ByteBuffer::getI32(usize index) const { return static_cast<i32>(getU32(index)); }
void ByteBuffer::putI32(usize index, i32 value) { putU32(index, static_cast<u32>(value)); }
i64 ByteBuffer::getI64(usize index) const { return static_cast<i64>(getU64(index)); }
void ByteBuffer::putI64(usize index, i64 value) { putU64(index, static_cast<u64>(value)); }

f32 ByteBuffer::getF32(usize index) const {
    u32 bits = getU32(index);
    f32 val;
    std::memcpy(&val, &bits, sizeof(f32));
    return val;
}

void ByteBuffer::putF32(usize index, f32 value) {
    u32 bits;
    std::memcpy(&bits, &value, sizeof(f32));
    putU32(index, bits);
}

f64 ByteBuffer::getF64(usize index) const {
    u64 bits = getU64(index);
    f64 val;
    std::memcpy(&val, &bits, sizeof(f64));
    return val;
}

void ByteBuffer::putF64(usize index, f64 value) {
    u64 bits;
    std::memcpy(&bits, &value, sizeof(f64));
    putU64(index, bits);
}

// ============================================================================
// 视图
// ============================================================================

ByteBuffer ByteBuffer::slice() const {
    ByteBuffer buf(remaining());
    std::memcpy(buf.data_, data_ + position_, remaining());
    buf.limit_ = remaining();
    buf.position_ = 0;
    return buf;
}

ByteBuffer ByteBuffer::duplicate() const {
    return ByteBuffer(*this);
}

// ============================================================================
// 容量管理
// ============================================================================

void ByteBuffer::reserve(usize newCapacity) {
    if (newCapacity > capacity_) {
        auto* newBuf = new u8[newCapacity];
        if (limit_ > 0) {
            std::memcpy(newBuf, data_, limit_);
        }
        if (owns_) {
            delete[] data_;
        }
        data_     = newBuf;
        capacity_ = newCapacity;
        owns_     = true;
    }
}

void ByteBuffer::shrinkToFit() {
    if (limit_ < capacity_) {
        usize newCap = limit_ > 0 ? limit_ : 1;
        auto* newBuf = new u8[newCap];
        if (limit_ > 0) {
            std::memcpy(newBuf, data_, limit_);
        }
        if (owns_) {
            delete[] data_;
        }
        data_     = newBuf;
        capacity_ = newCap;
        owns_     = true;
        if (limit_ > capacity_) {
            limit_ = capacity_;
        }
        if (position_ > limit_) {
            position_ = limit_;
        }
    }
}

// ============================================================================
// 替换 / 追加
// ============================================================================

void ByteBuffer::assign(const u8* data, usize size) {
    ensureWritable(size);
    std::memcpy(data_, data, size);
    position_ = size;
    limit_ = capacity_;  // keep limit at capacity for write mode
}

void ByteBuffer::append(const u8* data, usize size) {
    put(data, size);
}

void ByteBuffer::append(const ByteBuffer& other) {
    put(other);
}

// ============================================================================
// 交换 / 比较
// ============================================================================

void ByteBuffer::swap(ByteBuffer& other) noexcept {
    using std::swap;
    swap(data_,        other.data_);
    swap(capacity_,    other.capacity_);
    swap(position_,    other.position_);
    swap(limit_,       other.limit_);
    swap(mark_,        other.mark_);
    swap(order_,       other.order_);
    swap(owns_,        other.owns_);
    swap(markValid_,   other.markValid_);
}

bool ByteBuffer::equals(const ByteBuffer& other) const noexcept {
    usize thisSize  = remaining();
    usize otherSize = other.remaining();
    if (thisSize != otherSize) return false;
    if (thisSize == 0) return true;
    return std::memcmp(data_ + position_, other.data_ + other.position_, thisSize) == 0;
}

bool ByteBuffer::operator==(const ByteBuffer& other) const noexcept {
    return equals(other);
}

bool ByteBuffer::operator!=(const ByteBuffer& other) const noexcept {
    return !equals(other);
}

// ============================================================================
// 内部辅助
// ============================================================================

void ByteBuffer::grow(usize minCapacity) {
    usize newCap = capacity_ * 2;
    if (newCap < minCapacity) newCap = minCapacity;
    if (newCap < kDefaultCapacity) newCap = kDefaultCapacity;
    auto* newBuf = new u8[newCap];
    if (limit_ > 0) {
        std::memcpy(newBuf, data_, limit_);
    }
    if (owns_) {
        delete[] data_;
    }
    data_     = newBuf;
    capacity_ = newCap;
    owns_     = true;
}

void ByteBuffer::ensureWritable(usize needed) {
    usize required = position_ + needed;
    if (required <= capacity_) {
        if (required > limit_) {
            limit_ = required;  // push writable boundary
        }
        return;
    }
    grow(required);
    limit_ = required;
}

}  // namespace ca::core
