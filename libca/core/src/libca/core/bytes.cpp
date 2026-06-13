#include "bytes.hpp"

#include <algorithm>
#include <cstring>

namespace ca::core {

// ============================================================================
// Bytes
// ============================================================================

Bytes Bytes::from_static(const u8* data, usize len) {
    Bytes b;
    b.ptr_ = data;
    b.len_ = len;
    b.pos_ = 0;
    return b;
}

Bytes Bytes::copy_from_slice(const u8* data, usize len) {
    if (len == 0) return {};
    auto storage = std::shared_ptr<u8>(new u8[len], std::default_delete<u8[]>());
    std::memcpy(storage.get(), data, len);
    auto* raw = storage.get();
    return Bytes(raw, len, std::move(storage));
}

void Bytes::advance(usize cnt) {
    if (cnt > remaining()) throw std::out_of_range("Bytes::advance past end");
    pos_ += cnt;
}

Bytes Bytes::slice(usize begin, usize end) const {
    if (begin > end || end > len_) throw std::out_of_range("Bytes::slice invalid range");
    if (storage_) {
        return Bytes(ptr_ + begin, end - begin, storage_);
    }
    // static data
    Bytes b;
    b.ptr_ = ptr_ + begin;
    b.len_ = end - begin;
    return b;
}

// ── 类型化读（Byes） ──

u16 Bytes::get_u16_be() {
    if (pos_ + 2 > len_) throw std::out_of_range("Bytes::get_u16 underflow");
    u16 v = (static_cast<u16>(ptr_[pos_]) << 8) | static_cast<u16>(ptr_[pos_ + 1]);
    pos_ += 2;
    return v;
}

u16 Bytes::get_u16_le() {
    if (pos_ + 2 > len_) throw std::out_of_range("Bytes::get_u16_le underflow");
    u16 v = (static_cast<u16>(ptr_[pos_ + 1]) << 8) | static_cast<u16>(ptr_[pos_]);
    pos_ += 2;
    return v;
}

u32 Bytes::get_u32_be() {
    if (pos_ + 4 > len_) throw std::out_of_range("Bytes::get_u32 underflow");
    u32 v = (static_cast<u32>(ptr_[pos_])     << 24) |
            (static_cast<u32>(ptr_[pos_ + 1]) << 16) |
            (static_cast<u32>(ptr_[pos_ + 2]) <<  8) |
             static_cast<u32>(ptr_[pos_ + 3]);
    pos_ += 4;
    return v;
}

u32 Bytes::get_u32_le() {
    if (pos_ + 4 > len_) throw std::out_of_range("Bytes::get_u32_le underflow");
    u32 v = (static_cast<u32>(ptr_[pos_ + 3]) << 24) |
            (static_cast<u32>(ptr_[pos_ + 2]) << 16) |
            (static_cast<u32>(ptr_[pos_ + 1]) <<  8) |
             static_cast<u32>(ptr_[pos_]);
    pos_ += 4;
    return v;
}

u64 Bytes::get_u64_be() {
    if (pos_ + 8 > len_) throw std::out_of_range("Bytes::get_u64 underflow");
    u64 v = (static_cast<u64>(ptr_[pos_])     << 56) |
            (static_cast<u64>(ptr_[pos_ + 1]) << 48) |
            (static_cast<u64>(ptr_[pos_ + 2]) << 40) |
            (static_cast<u64>(ptr_[pos_ + 3]) << 32) |
            (static_cast<u64>(ptr_[pos_ + 4]) << 24) |
            (static_cast<u64>(ptr_[pos_ + 5]) << 16) |
            (static_cast<u64>(ptr_[pos_ + 6]) <<  8) |
             static_cast<u64>(ptr_[pos_ + 7]);
    pos_ += 8;
    return v;
}

u64 Bytes::get_u64_le() {
    if (pos_ + 8 > len_) throw std::out_of_range("Bytes::get_u64_le underflow");
    u64 v = (static_cast<u64>(ptr_[pos_ + 7]) << 56) |
            (static_cast<u64>(ptr_[pos_ + 6]) << 48) |
            (static_cast<u64>(ptr_[pos_ + 5]) << 40) |
            (static_cast<u64>(ptr_[pos_ + 4]) << 32) |
            (static_cast<u64>(ptr_[pos_ + 3]) << 24) |
            (static_cast<u64>(ptr_[pos_ + 2]) << 16) |
            (static_cast<u64>(ptr_[pos_ + 1]) <<  8) |
             static_cast<u64>(ptr_[pos_]);
    pos_ += 8;
    return v;
}

i16 Bytes::get_i16_be() { return static_cast<i16>(get_u16_be()); }
i16 Bytes::get_i16_le() { return static_cast<i16>(get_u16_le()); }
i32 Bytes::get_i32_be() { return static_cast<i32>(get_u32_be()); }
i32 Bytes::get_i32_le() { return static_cast<i32>(get_u32_le()); }
i64 Bytes::get_i64_be() { return static_cast<i64>(get_u64_be()); }
i64 Bytes::get_i64_le() { return static_cast<i64>(get_u64_le()); }

f32 Bytes::get_f32_be() {
    u32 bits = get_u32_be();
    f32 v;
    std::memcpy(&v, &bits, sizeof(f32));
    return v;
}

f64 Bytes::get_f64_be() {
    u64 bits = get_u64_be();
    f64 v;
    std::memcpy(&v, &bits, sizeof(f64));
    return v;
}


// ============================================================================
// BytesMut
// ============================================================================

BytesMut BytesMut::with_capacity(usize cap) {
    BytesMut b;
    if (cap > 0) {
        b.data_ = std::unique_ptr<u8[]>(new u8[cap]);
        b.capacity_ = cap;
    }
    return b;
}

BytesMut::BytesMut(const BytesMut& other)
    : data_(other.len_ > 0 ? new u8[other.len_] : nullptr),
      len_(other.len_),
      capacity_(other.len_),
      pos_(other.pos_) {
    if (len_ > 0) {
        std::memcpy(data_.get(), other.data_.get(), len_);
    }
}

BytesMut& BytesMut::operator=(const BytesMut& other) {
    if (this != &other) {
        auto new_data = other.len_ > 0 ? std::unique_ptr<u8[]>(new u8[other.len_]) : nullptr;
        if (other.len_ > 0) {
            std::memcpy(new_data.get(), other.data_.get(), other.len_);
        }
        data_ = std::move(new_data);
        len_ = other.len_;
        capacity_ = other.len_;
        pos_ = other.pos_;
    }
    return *this;
}

void BytesMut::advance(usize cnt) {
    if (cnt > remaining()) throw std::out_of_range("BytesMut::advance past end");
    pos_ += cnt;
}

void BytesMut::reserve(usize additional) {
    usize needed = len_ + additional;
    if (needed > capacity_) {
        grow(needed);
    }
}

void BytesMut::put_slice(const u8* data, usize len) {
    if (len == 0) return;
    ensure_writable(len);
    std::memcpy(data_.get() + len_, data, len);
    len_ += len;
}

// ── 类型化写（BytesMut） ──

void BytesMut::put_u16_be(u16 val) {
    ensure_writable(2);
    data_[len_]     = static_cast<u8>((val >> 8) & 0xFF);
    data_[len_ + 1] = static_cast<u8>(val & 0xFF);
    len_ += 2;
}

void BytesMut::put_u16_le(u16 val) {
    ensure_writable(2);
    data_[len_]     = static_cast<u8>(val & 0xFF);
    data_[len_ + 1] = static_cast<u8>((val >> 8) & 0xFF);
    len_ += 2;
}

void BytesMut::put_u32_be(u32 val) {
    ensure_writable(4);
    data_[len_]     = static_cast<u8>((val >> 24) & 0xFF);
    data_[len_ + 1] = static_cast<u8>((val >> 16) & 0xFF);
    data_[len_ + 2] = static_cast<u8>((val >>  8) & 0xFF);
    data_[len_ + 3] = static_cast<u8>(val & 0xFF);
    len_ += 4;
}

void BytesMut::put_u32_le(u32 val) {
    ensure_writable(4);
    data_[len_]     = static_cast<u8>(val & 0xFF);
    data_[len_ + 1] = static_cast<u8>((val >>  8) & 0xFF);
    data_[len_ + 2] = static_cast<u8>((val >> 16) & 0xFF);
    data_[len_ + 3] = static_cast<u8>((val >> 24) & 0xFF);
    len_ += 4;
}

void BytesMut::put_u64_be(u64 val) {
    ensure_writable(8);
    data_[len_]     = static_cast<u8>((val >> 56) & 0xFF);
    data_[len_ + 1] = static_cast<u8>((val >> 48) & 0xFF);
    data_[len_ + 2] = static_cast<u8>((val >> 40) & 0xFF);
    data_[len_ + 3] = static_cast<u8>((val >> 32) & 0xFF);
    data_[len_ + 4] = static_cast<u8>((val >> 24) & 0xFF);
    data_[len_ + 5] = static_cast<u8>((val >> 16) & 0xFF);
    data_[len_ + 6] = static_cast<u8>((val >>  8) & 0xFF);
    data_[len_ + 7] = static_cast<u8>(val & 0xFF);
    len_ += 8;
}

void BytesMut::put_u64_le(u64 val) {
    ensure_writable(8);
    data_[len_]     = static_cast<u8>(val & 0xFF);
    data_[len_ + 1] = static_cast<u8>((val >>  8) & 0xFF);
    data_[len_ + 2] = static_cast<u8>((val >> 16) & 0xFF);
    data_[len_ + 3] = static_cast<u8>((val >> 24) & 0xFF);
    data_[len_ + 4] = static_cast<u8>((val >> 32) & 0xFF);
    data_[len_ + 5] = static_cast<u8>((val >> 40) & 0xFF);
    data_[len_ + 6] = static_cast<u8>((val >> 48) & 0xFF);
    data_[len_ + 7] = static_cast<u8>((val >> 56) & 0xFF);
    len_ += 8;
}

void BytesMut::put_i16_be(i16 val) { put_u16_be(static_cast<u16>(val)); }
void BytesMut::put_i16_le(i16 val) { put_u16_le(static_cast<u16>(val)); }
void BytesMut::put_i32_be(i32 val) { put_u32_be(static_cast<u32>(val)); }
void BytesMut::put_i32_le(i32 val) { put_u32_le(static_cast<u32>(val)); }
void BytesMut::put_i64_be(i64 val) { put_u64_be(static_cast<u64>(val)); }
void BytesMut::put_i64_le(i64 val) { put_u64_le(static_cast<u64>(val)); }

void BytesMut::put_f32_be(f32 val) {
    u32 bits;
    std::memcpy(&bits, &val, sizeof(f32));
    put_u32_be(bits);
}

void BytesMut::put_f64_be(f64 val) {
    u64 bits;
    std::memcpy(&bits, &val, sizeof(f64));
    put_u64_be(bits);
}

// ── 类型化读（BytesMut） ──

u16 BytesMut::get_u16_be() {
    if (pos_ + 2 > len_) throw std::out_of_range("BytesMut::get_u16 underflow");
    u16 v = (static_cast<u16>(data_[pos_]) << 8) | static_cast<u16>(data_[pos_ + 1]);
    pos_ += 2;
    return v;
}

u16 BytesMut::get_u16_le() {
    if (pos_ + 2 > len_) throw std::out_of_range("BytesMut::get_u16_le underflow");
    u16 v = (static_cast<u16>(data_[pos_ + 1]) << 8) | static_cast<u16>(data_[pos_]);
    pos_ += 2;
    return v;
}

u32 BytesMut::get_u32_be() {
    if (pos_ + 4 > len_) throw std::out_of_range("BytesMut::get_u32 underflow");
    u32 v = (static_cast<u32>(data_[pos_])     << 24) |
            (static_cast<u32>(data_[pos_ + 1]) << 16) |
            (static_cast<u32>(data_[pos_ + 2]) <<  8) |
             static_cast<u32>(data_[pos_ + 3]);
    pos_ += 4;
    return v;
}

u32 BytesMut::get_u32_le() {
    if (pos_ + 4 > len_) throw std::out_of_range("BytesMut::get_u32_le underflow");
    u32 v = (static_cast<u32>(data_[pos_ + 3]) << 24) |
            (static_cast<u32>(data_[pos_ + 2]) << 16) |
            (static_cast<u32>(data_[pos_ + 1]) <<  8) |
             static_cast<u32>(data_[pos_]);
    pos_ += 4;
    return v;
}

u64 BytesMut::get_u64_be() {
    if (pos_ + 8 > len_) throw std::out_of_range("BytesMut::get_u64 underflow");
    u64 v = (static_cast<u64>(data_[pos_])     << 56) |
            (static_cast<u64>(data_[pos_ + 1]) << 48) |
            (static_cast<u64>(data_[pos_ + 2]) << 40) |
            (static_cast<u64>(data_[pos_ + 3]) << 32) |
            (static_cast<u64>(data_[pos_ + 4]) << 24) |
            (static_cast<u64>(data_[pos_ + 5]) << 16) |
            (static_cast<u64>(data_[pos_ + 6]) <<  8) |
             static_cast<u64>(data_[pos_ + 7]);
    pos_ += 8;
    return v;
}

u64 BytesMut::get_u64_le() {
    if (pos_ + 8 > len_) throw std::out_of_range("BytesMut::get_u64_le underflow");
    u64 v = (static_cast<u64>(data_[pos_ + 7]) << 56) |
            (static_cast<u64>(data_[pos_ + 6]) << 48) |
            (static_cast<u64>(data_[pos_ + 5]) << 40) |
            (static_cast<u64>(data_[pos_ + 4]) << 32) |
            (static_cast<u64>(data_[pos_ + 3]) << 24) |
            (static_cast<u64>(data_[pos_ + 2]) << 16) |
            (static_cast<u64>(data_[pos_ + 1]) <<  8) |
             static_cast<u64>(data_[pos_]);
    pos_ += 8;
    return v;
}

i16 BytesMut::get_i16_be() { return static_cast<i16>(get_u16_be()); }
i16 BytesMut::get_i16_le() { return static_cast<i16>(get_u16_le()); }
i32 BytesMut::get_i32_be() { return static_cast<i32>(get_u32_be()); }
i32 BytesMut::get_i32_le() { return static_cast<i32>(get_u32_le()); }
i64 BytesMut::get_i64_be() { return static_cast<i64>(get_u64_be()); }
i64 BytesMut::get_i64_le() { return static_cast<i64>(get_u64_le()); }

f32 BytesMut::get_f32_be() {
    u32 bits = get_u32_be();
    f32 v;
    std::memcpy(&v, &bits, sizeof(f32));
    return v;
}

f64 BytesMut::get_f64_be() {
    u64 bits = get_u64_be();
    f64 v;
    std::memcpy(&v, &bits, sizeof(f64));
    return v;
}

// ── 冻结 ──

Bytes BytesMut::freeze() {
    if (len_ == 0) return {};
    // transfer ownership from unique_ptr to shared_ptr
    auto shared = std::shared_ptr<u8>(data_.release(), std::default_delete<u8[]>());
    auto* raw = shared.get();
    Bytes b(raw, len_, std::move(shared));
    len_ = 0;
    capacity_ = 0;
    pos_ = 0;
    return b;
}

// ── 比较 ──

bool BytesMut::equals(const BytesMut& other) const noexcept {
    usize this_rem = remaining();
    if (this_rem != other.remaining()) return false;
    if (this_rem == 0) return true;
    return std::memcmp(data_.get() + pos_, other.data_.get() + other.pos_, this_rem) == 0;
}

// ── 内部辅助 ──

void BytesMut::ensure_writable(usize needed) {
    usize required = len_ + needed;
    if (required <= capacity_) return;
    grow(required);
}

void BytesMut::grow(usize min_capacity) {
    usize new_cap = capacity_ * 2;
    if (new_cap < min_capacity) new_cap = min_capacity;
    if (new_cap < 32) new_cap = 32;
    auto new_data = std::unique_ptr<u8[]>(new u8[new_cap]);
    if (len_ > 0) {
        std::memcpy(new_data.get(), data_.get(), len_);
    }
    data_ = std::move(new_data);
    capacity_ = new_cap;
}

}  // namespace ca::core
