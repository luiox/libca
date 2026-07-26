#pragma once

#include "datatype.hpp"
#include "result.hpp"

#include <cstring>
#include <memory>
#include <stdexcept>

/// @file bytes.hpp
/// @brief 字节序列三件套：ByteSlice（非拥有视图）、Bytes（不可变共享）、BytesMut（可变缓冲）。
///        适合协议解析与序列化。类型化读写显式区分端序（_be 大端/network、_le 小端）。
/// @note 错误模型分两类，对齐全库 Result：
///       - **读游标类**（get_*/advance/copy_to_slice）：输入截断是解析的正常预期，
///         返回 `Result<T, BytesError>`，剩余不足时得到 `Err(BytesError::Underflow)`，不抛异常。
///       - **索引越界类**（slice/sub_slice 给了非法下标）：属调用方编程错误，仍抛 std::out_of_range。
///       - 分配尺寸溢出（reserve/ensure_writable）：抛 std::length_error（极端防御，正常不可达）。

namespace ca::core {

class BytesMut;

/// @brief 字节读取错误。目前只有一种：剩余字节不足以完成本次读取。
enum class BytesError {
    Underflow,  ///< 剩余可读字节不足（输入被截断）
};

/// FsError 风格：转可读字符串，便于日志。
const char* to_cstr(BytesError e) noexcept;

/// @brief 非拥有只读字节视图，对标 Rust &[u8]。
/// @warning 不持有数据，调用方须保证底层数据生命周期覆盖视图使用期。
class ByteSlice {
public:
    /// 空视图。
    ByteSlice() noexcept = default;
    /// 从指针+长度构造（不复制、不接管所有权）。
    ByteSlice(const u8* data, usize len) noexcept : data_(data), len_(len) {}

    const u8* data() const noexcept { return data_; }  ///< 底层指针
    usize size() const noexcept { return len_; }       ///< 字节数
    bool  empty() const noexcept { return len_ == 0; } ///< 是否为空

    /// 无边界检查的下标访问。
    const u8& operator[](usize index) const { return data_[index]; }

    /// @brief 返回子视图 [start, start+count)。@throw std::out_of_range 越界。
    ByteSlice sub_slice(usize start, usize count) const {
        if (start > len_ || count > len_ - start)
            throw std::out_of_range("ByteSlice::sub_slice invalid range");
        return ByteSlice(data_ + start, count);
    }

private:
    const u8* data_{nullptr};
    usize     len_{0};
};

/// @brief 不可变字节序列，引用计数共享存储，支持零拷贝切片与读游标。
/// @note 拷贝是浅拷贝（共享存储）；读取类接口会前进内部读游标。
class Bytes {
public:
    // ── 工厂 ──
    /// 引用外部静态数据，不复制（调用方须保证数据长期有效）。
    static Bytes from_static(const u8* data, usize len);
    /// 复制一份数据，自管理生命周期。
    static Bytes copy_from_slice(const u8* data, usize len);

    Bytes() noexcept = default;
    Bytes(const Bytes& other) = default;
    Bytes(Bytes&& other) noexcept = default;
    Bytes& operator=(const Bytes& other) = default;
    Bytes& operator=(Bytes&& other) noexcept = default;

    // ── 查询 ──
    usize len() const noexcept;        ///< 总长度
    bool  is_empty() const noexcept;   ///< 是否为空
    const u8* as_ptr() const noexcept; ///< 当前读位置指针

    // ── 读游标 ──
    usize remaining() const noexcept;  ///< 剩余可读字节
    /// @brief 前进读游标 cnt 字节。剩余不足返回 Err(Underflow)，游标不动。
    Result<void, BytesError> advance(usize cnt);

    // ── 零拷贝切片 ──
    /// @brief 返回 [begin, end) 的零拷贝切片（共享存储）。
    /// @throw std::out_of_range 下标非法（编程错误，非输入截断）。
    Bytes slice(usize begin, usize end) const;

    // ── 类型化读（前进游标，后缀 _be = 大端/network order，_le = 小端） ──
    // 剩余不足返回 Err(BytesError::Underflow)，游标不动。
    Result<u8,  BytesError> get_u8();
    Result<u16, BytesError> get_u16_be();
    Result<u16, BytesError> get_u16_le();
    Result<u32, BytesError> get_u32_be();
    Result<u32, BytesError> get_u32_le();
    Result<u64, BytesError> get_u64_be();
    Result<u64, BytesError> get_u64_le();
    Result<i16, BytesError> get_i16_be();
    Result<i16, BytesError> get_i16_le();
    Result<i32, BytesError> get_i32_be();
    Result<i32, BytesError> get_i32_le();
    Result<i64, BytesError> get_i64_be();
    Result<i64, BytesError> get_i64_le();
    Result<f32, BytesError> get_f32_be();
    Result<f64, BytesError> get_f64_be();

    // ── 批量读 ──
    /// @brief 复制 len 字节到 dst 并前进游标。剩余不足返回 Err(Underflow)，不复制、游标不动。
    Result<void, BytesError> copy_to_slice(u8* dst, usize len);

private:
    friend class BytesMut;

    Bytes(const u8* data, usize len, std::shared_ptr<u8> storage) noexcept;

    const u8* ptr_{nullptr};
    usize     len_{0};
    usize     pos_{0};
    std::shared_ptr<u8> storage_;
};


/// @brief 可变字节缓冲区，唯一所有权。写入追加到尾部，读取用独立读游标。
/// @note 拷贝是深拷贝；freeze() 转为不可变 Bytes 后原对象清空。
class BytesMut {
public:
    // ── 工厂 ──
    /// 预分配 cap 字节容量的空缓冲。
    static BytesMut with_capacity(usize cap);

    BytesMut() noexcept = default;
    BytesMut(const BytesMut& other);
    BytesMut(BytesMut&& other) noexcept = default;
    BytesMut& operator=(const BytesMut& other);
    BytesMut& operator=(BytesMut&& other) noexcept = default;
    ~BytesMut() = default;

    // ── 查询 ──
    usize len() const noexcept;         ///< 已写入长度
    bool  is_empty() const noexcept;    ///< 是否为空
    const u8* as_ptr() const noexcept;  ///< 当前读位置只读指针
    u8*  as_mut_ptr() const noexcept;   ///< 当前读位置可写指针

    // ── 读游标 ──
    usize remaining() const noexcept;   ///< 剩余可读字节
    /// @brief 前进读游标 cnt 字节。剩余不足返回 Err(Underflow)，游标不动。
    Result<void, BytesError> advance(usize cnt);

    // ── 写剩余空间 ──
    usize remaining_mut() const noexcept; ///< 剩余可写容量

    // ── 容量管理 ──
    void reserve(usize additional);     ///< 确保还能再写入 additional 字节
    void clear() noexcept;              ///< 清空内容并重置读游标
    void truncate(usize len);           ///< 截断到指定长度

    // ── 批量写 ──
    void put_slice(const u8* data, usize len); ///< 追加写入 len 字节

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

    // ── 类型化读（前进游标）。剩余不足返回 Err(BytesError::Underflow)，游标不动。 ──
    Result<u8,  BytesError> get_u8();
    Result<u16, BytesError> get_u16_be();
    Result<u16, BytesError> get_u16_le();
    Result<u32, BytesError> get_u32_be();
    Result<u32, BytesError> get_u32_le();
    Result<u64, BytesError> get_u64_be();
    Result<u64, BytesError> get_u64_le();
    Result<i16, BytesError> get_i16_be();
    Result<i16, BytesError> get_i16_le();
    Result<i32, BytesError> get_i32_be();
    Result<i32, BytesError> get_i32_le();
    Result<i64, BytesError> get_i64_be();
    Result<i64, BytesError> get_i64_le();
    Result<f32, BytesError> get_f32_be();
    Result<f64, BytesError> get_f64_be();

    // ── 冻结为不可变 Bytes ──
    /// @brief 转为不可变 Bytes，转移所有权；调用后本对象清空。
    Bytes freeze();

    // ── 比较 ──
    /// 比较双方"剩余可读内容"是否相等。
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

inline Result<u8, BytesError> Bytes::get_u8() {
    if (pos_ + 1 > len_) return Err(BytesError::Underflow);
    return Ok(ptr_[pos_++]);
}

inline Result<void, BytesError> Bytes::copy_to_slice(u8* dst, usize len) {
    // 用减法比较避免 pos_ + len 溢出回绕（pos_ <= len_ 恒成立，len_ - pos_ 安全）。
    if (len > len_ - pos_) return Err(BytesError::Underflow);
    std::memcpy(dst, ptr_ + pos_, len);
    pos_ += len;
    return Ok();
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

inline Result<u8, BytesError> BytesMut::get_u8() {
    if (pos_ + 1 > len_) return Err(BytesError::Underflow);
    return Ok(data_[pos_++]);
}

inline bool BytesMut::operator==(const BytesMut& other) const noexcept { return equals(other); }
inline bool BytesMut::operator!=(const BytesMut& other) const noexcept { return !equals(other); }

}  // namespace ca::core
