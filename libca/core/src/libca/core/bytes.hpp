#pragma once

#include <libca/core/datatype.hpp>

#include <cstring>
#include <memory>
#include <stdexcept>

namespace ca::core {

class BytesMut;

/// 非拥有字节视图，对标 Rust &[u8]
class ByteSlice {
public:
    ByteSlice() noexcept = default;
    ByteSlice(const u8* data, usize len) noexcept : data_(data), len_(len) {}

    const u8* data() const noexcept { return data_; }
    usize size() const noexcept { return len_; }
    bool  empty() const noexcept { return len_ == 0; }

    const u8& operator[](usize index) const { return data_[index]; }

    ByteSlice sub_slice(usize start, usize count) const {
        if (start > len_ || count > len_ - start)
            throw std::out_of_range("ByteSlice::sub_slice invalid range");
        return ByteSlice(data_ + start, count);
    }

private:
    const u8* data_{nullptr};
    usize     len_{0};
};

/// 不可变字节序列，引用计数共享存储，支持零拷贝切片
class Bytes {
public:
    // ── 工厂 ──
    static Bytes from_static(const u8* data, usize len);
    static Bytes copy_from_slice(const u8* data, usize len);

    Bytes() noexcept = default;
    Bytes(const Bytes& other) = default;
    Bytes(Bytes&& other) noexcept = default;
    Bytes& operator=(const Bytes& other) = default;
    Bytes& operator=(Bytes&& other) noexcept = default;

    // ── 查询 ──
    usize len() const noexcept;
    bool  is_empty() const noexcept;
    const u8* as_ptr() const noexcept;

    // ── 读游标 ──
    usize remaining() const noexcept;
    void  advance(usize cnt);

    // ── 零拷贝切片 ──
    Bytes slice(usize begin, usize end) const;

    // ── 类型化读（前进游标，后缀 _be = 大端/network order，_le = 小端） ──
    u8   get_u8();
    u16  get_u16_be();
    u16  get_u16_le();
    u32  get_u32_be();
    u32  get_u32_le();
    u64  get_u64_be();
    u64  get_u64_le();
    i16  get_i16_be();
    i16  get_i16_le();
    i32  get_i32_be();
    i32  get_i32_le();
    i64  get_i64_be();
    i64  get_i64_le();
    f32  get_f32_be();
    f64  get_f64_be();

    // ── 批量读 ──
    void copy_to_slice(u8* dst, usize len);

private:
    friend class BytesMut;

    Bytes(const u8* data, usize len, std::shared_ptr<u8> storage) noexcept;

    const u8* ptr_{nullptr};
    usize     len_{0};
    usize     pos_{0};
    std::shared_ptr<u8> storage_;
};


/// 可变字节缓冲区，唯一所有权，可写入/读取/冻结为 Bytes
class BytesMut {
public:
    // ── 工厂 ──
    static BytesMut with_capacity(usize cap);

    BytesMut() noexcept = default;
    BytesMut(const BytesMut& other);
    BytesMut(BytesMut&& other) noexcept = default;
    BytesMut& operator=(const BytesMut& other);
    BytesMut& operator=(BytesMut&& other) noexcept = default;
    ~BytesMut() = default;

    // ── 查询 ──
    usize len() const noexcept;
    bool  is_empty() const noexcept;
    const u8* as_ptr() const noexcept;
    u8*  as_mut_ptr() const noexcept;

    // ── 读游标 ──
    usize remaining() const noexcept;
    void  advance(usize cnt);

    // ── 写剩余空间 ──
    usize remaining_mut() const noexcept;

    // ── 容量管理 ──
    void reserve(usize additional);
    void clear() noexcept;
    void truncate(usize len);

    // ── 批量写 ──
    void put_slice(const u8* data, usize len);

    // ── 类型化写（后缀 _be = 大端/network order，_le = 小端） ──
    void put_u8(u8 val);
    void put_u16_be(u16 val);
    void put_u16_le(u16 val);
    void put_u32_be(u32 val);
    void put_u32_le(u32 val);
    void put_u64_be(u64 val);
    void put_u64_le(u64 val);
    void put_i16_be(i16 val);
    void put_i16_le(i16 val);
    void put_i32_be(i32 val);
    void put_i32_le(i32 val);
    void put_i64_be(i64 val);
    void put_i64_le(i64 val);
    void put_f32_be(f32 val);
    void put_f64_be(f64 val);

    // ── 类型化读（前进游标） ──
    u8   get_u8();
    u16  get_u16_be();
    u16  get_u16_le();
    u32  get_u32_be();
    u32  get_u32_le();
    u64  get_u64_be();
    u64  get_u64_le();
    i16  get_i16_be();
    i16  get_i16_le();
    i32  get_i32_be();
    i32  get_i32_le();
    i64  get_i64_be();
    i64  get_i64_le();
    f32  get_f32_be();
    f64  get_f64_be();

    // ── 冻结为不可变 Bytes ──
    Bytes freeze();

    // ── 比较 ──
    bool equals(const BytesMut& other) const noexcept;
    bool operator==(const BytesMut& other) const noexcept;
    bool operator!=(const BytesMut& other) const noexcept;

private:
    void ensure_writable(usize needed);
    void grow(usize min_capacity);

    std::unique_ptr<u8[]> data_;
    usize len_{0};
    usize capacity_{0};
    usize pos_{0};
};


// ============================================================================
// Bytes 内联实现
// ============================================================================

inline Bytes::Bytes(const u8* data, usize len, std::shared_ptr<u8> storage) noexcept
    : ptr_(data), len_(len), pos_(0), storage_(std::move(storage)) {}

inline usize Bytes::len() const noexcept { return len_; }
inline bool Bytes::is_empty() const noexcept { return len_ == 0; }
inline const u8* Bytes::as_ptr() const noexcept { return ptr_ + pos_; }
inline usize Bytes::remaining() const noexcept { return len_ - pos_; }

inline u8 Bytes::get_u8() {
    if (pos_ + 1 > len_) throw std::out_of_range("Bytes::get_u8 underflow");
    return ptr_[pos_++];
}

inline void Bytes::copy_to_slice(u8* dst, usize len) {
    if (pos_ + len > len_) throw std::out_of_range("Bytes::copy_to_slice underflow");
    std::memcpy(dst, ptr_ + pos_, len);
    pos_ += len;
}


// ============================================================================
// BytesMut 内联实现
// ============================================================================

inline usize BytesMut::len() const noexcept { return len_; }
inline bool BytesMut::is_empty() const noexcept { return len_ == 0; }
inline const u8* BytesMut::as_ptr() const noexcept { return data_.get() + pos_; }
inline u8* BytesMut::as_mut_ptr() const noexcept { return data_.get() + pos_; }

inline usize BytesMut::remaining() const noexcept { return len_ - pos_; }
inline usize BytesMut::remaining_mut() const noexcept { return capacity_ - len_; }

inline void BytesMut::clear() noexcept { len_ = 0; pos_ = 0; }

inline void BytesMut::truncate(usize new_len) {
    if (new_len < len_) { len_ = new_len; if (pos_ > len_) pos_ = len_; }
}

inline void BytesMut::put_u8(u8 val) {
    ensure_writable(1);
    data_[len_++] = val;
}

inline u8 BytesMut::get_u8() {
    if (pos_ + 1 > len_) throw std::out_of_range("BytesMut::get_u8 underflow");
    return data_[pos_++];
}

inline bool BytesMut::operator==(const BytesMut& other) const noexcept { return equals(other); }
inline bool BytesMut::operator!=(const BytesMut& other) const noexcept { return !equals(other); }

}  // namespace ca::core
