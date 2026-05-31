//
// @brief Java-style 游标字节缓冲区 (ByteBuffer)
// @author Canrad
// @date 2026/05/31
// @note 参考 java.nio.ByteBuffer 的 position/limit/capacity 游标模型，
//       基于 u8 类型存储。拆分为 .hpp 声明 + .cpp 实现。
//       命名空间 ca::core
//

#ifndef LIBCA_CORE_BYTE_BUFFER_HPP
#define LIBCA_CORE_BYTE_BUFFER_HPP

#include <libca/core/datatype.hpp>

#include <cstring>
#include <stdexcept>
#include <utility>

namespace ca::core {

/// 字节序枚举
enum class ByteOrder {
    BigEndian,
    LittleEndian
};

/// 游标字节缓冲区
class ByteBuffer {
public:
    // ── 工厂方法 ──
    static ByteBuffer allocate(usize capacity);
    static ByteBuffer wrap(u8* data, usize size);
    static ByteBuffer copyOf(const u8* data, usize size);

    // ── 构造/析构/赋值 ──
    ByteBuffer() noexcept;
    explicit ByteBuffer(usize capacity);
    ByteBuffer(const u8* data, usize size);
    ByteBuffer(const ByteBuffer& other);
    ByteBuffer(ByteBuffer&& other) noexcept;
    ~ByteBuffer();

    ByteBuffer& operator=(const ByteBuffer& other);
    ByteBuffer& operator=(ByteBuffer&& other) noexcept;

    // ── 游标控制 ──
    usize position() const noexcept;
    void position(usize newPosition);
    usize limit() const noexcept;
    void limit(usize newLimit);
    usize capacity() const noexcept;
    usize remaining() const noexcept;
    bool   hasRemaining() const noexcept;

    void flip();
    void rewind();
    void clear() noexcept;
    void compact();

    void mark();
    void reset();

    // ── 字节序 ──
    ByteOrder order() const noexcept;
    void order(ByteOrder order) noexcept;

    // ── 相对读写 (position 自动前进) ──
    u8 get();
    void put(u8 b);
    void get(u8* dst, usize length);
    void put(const u8* src, usize length);
    void put(const ByteBuffer& src);

    // ── 绝对读写 (不改变 position) ──
    u8 get(usize index) const;
    void put(usize index, u8 b);
    void get(usize index, u8* dst, usize length) const;
    void put(usize index, const u8* src, usize length);

    // ── 类型化读写 — 相对 ──
    u16 getU16();
    void putU16(u16 value);
    u32 getU32();
    void putU32(u32 value);
    u64 getU64();
    void putU64(u64 value);
    i16 getI16();
    void putI16(i16 value);
    i32 getI32();
    void putI32(i32 value);
    i64 getI64();
    void putI64(i64 value);
    f32 getF32();
    void putF32(f32 value);
    f64 getF64();
    void putF64(f64 value);

    // ── 类型化读写 — 绝对 ──
    u16 getU16(usize index) const;
    void putU16(usize index, u16 value);
    u32 getU32(usize index) const;
    void putU32(usize index, u32 value);
    u64 getU64(usize index) const;
    void putU64(usize index, u64 value);
    i16 getI16(usize index) const;
    void putI16(usize index, i16 value);
    i32 getI32(usize index) const;
    void putI32(usize index, i32 value);
    i64 getI64(usize index) const;
    void putI64(usize index, i64 value);
    f32 getF32(usize index) const;
    void putF32(usize index, f32 value);
    f64 getF64(usize index) const;
    void putF64(usize index, f64 value);

    // ── 视图 ──
    ByteBuffer slice() const;
    ByteBuffer duplicate() const;

    // ── 原始指针 ──
    const u8* data() const noexcept;
    u8* data() noexcept;

    // ── 索引访问 ──
    u8  operator[](usize index) const noexcept;
    u8& operator[](usize index) noexcept;
    u8  at(usize index) const;
    u8& at(usize index);
    u8  front() const;
    u8& front();
    u8  back() const;
    u8& back();

    // ── 大小状态 ──
    usize size() const noexcept;   // 返回 limit
    bool   empty() const noexcept; // remaining() == 0

    // ── 容量管理 ──
    void reserve(usize newCapacity);
    void shrinkToFit();

    // ── 替换/追加 ──
    void assign(const u8* data, usize size);
    void append(const u8* data, usize size);
    void append(const ByteBuffer& other);

    // ── 交换/比较 ──
    void swap(ByteBuffer& other) noexcept;
    bool equals(const ByteBuffer& other) const noexcept;
    bool operator==(const ByteBuffer& other) const noexcept;
    bool operator!=(const ByteBuffer& other) const noexcept;

    // ── 兼容别名 ──
    static ByteBuffer fromData(const u8* data, usize size) { return copyOf(data, size); }
    static ByteBuffer withCapacity(usize capacity) { return allocate(capacity); }

private:
    u8*       data_{nullptr};
    usize     capacity_{0};
    usize     position_{0};
    usize     limit_{0};
    usize     mark_{0};
    ByteOrder order_{ByteOrder::BigEndian};
    bool      owns_{true};
    bool      markValid_{false};
    static constexpr usize kDefaultCapacity = 32;

    void grow(usize minCapacity);
    void ensureWritable(usize needed);
};


// ============================================================================
// 内联实现（仅极简 getter / trivial 操作）
// ============================================================================

inline ByteOrder ByteBuffer::order() const noexcept { return order_; }
inline void ByteBuffer::order(ByteOrder order) noexcept { order_ = order; }

inline usize ByteBuffer::position() const noexcept { return position_; }
inline usize ByteBuffer::limit() const noexcept { return limit_; }
inline usize ByteBuffer::capacity() const noexcept { return capacity_; }
inline usize ByteBuffer::remaining() const noexcept { return limit_ - position_; }
inline bool   ByteBuffer::hasRemaining() const noexcept { return position_ < limit_; }

inline usize ByteBuffer::size() const noexcept { return limit_; }
inline bool   ByteBuffer::empty() const noexcept { return position_ >= limit_; }

inline const u8* ByteBuffer::data() const noexcept { return data_; }
inline u8* ByteBuffer::data() noexcept { return data_; }

inline u8 ByteBuffer::operator[](usize index) const noexcept { return data_[index]; }
inline u8& ByteBuffer::operator[](usize index) noexcept { return data_[index]; }

inline u8 ByteBuffer::front() const { return data_[0]; }
inline u8& ByteBuffer::front() { return data_[0]; }
inline u8 ByteBuffer::back() const { return data_[limit_ - 1]; }
inline u8& ByteBuffer::back() { return data_[limit_ - 1]; }

}  // namespace ca::core

#endif  // LIBCA_CORE_BYTE_BUFFER_HPP
