//
// @brief 引用计数 UTF-8 字符串池 (Utf8StringPool) 及池化指针 (Utf8StringPooledPtr)
// @author Canrad
// @date 2026/05/31
// @note 非线程安全。Pool 必须 outlive 所有 PooledPtr。
//       PooledPtr 拷贝时原子递增 ref_count，析构时递减到 0 自动释放数据。
//
// 使用场景:
//   对象寿命不一致的场景，每个字符串独立管理生存期。
//   Utf8StringPool pool;
//   auto s1 = pool.intern("hello");  // ref_count=1
//   auto s2 = s1;                    // ref_count=2
//   // s2 析构 → ref_count=1
//   // s1 析构 → ref_count=0 → 释放内存
//

#ifndef LIBCA_STR_UTF8_STRING_POOL_HPP
#define LIBCA_STR_UTF8_STRING_POOL_HPP

#include "libca/core/datatype.hpp"
#include "utf8_string.hpp"

#include <cstddef>
#include <cstring>
#include <unordered_map>
#include <list>
#include <vector>

namespace ca::str {

// ============================================================================
// 前向声明
// ============================================================================

class Utf8StringPool;


// ============================================================================
// Utf8PoolEntry — 池内条目（数据 + 引用计数）
// ============================================================================

struct Utf8PoolEntry {
    u8*   data;
    usize byte_length;
    usize length;    // 码点个数
    usize hash;
    usize ref_count;
    bool  alive;
};


// ============================================================================
// Utf8StringPooledPtr — 引用计数的池化不可变 UTF-8 字符串指针
// ============================================================================
//
// 8 字节（一个指针），指向 Pool 内部的 PoolEntry。
// 拷贝/赋值递增 ref_count，析构递减，到 0 自动释放数据。
//

class Utf8StringPooledPtr {
public:
    // 默认构造：空指针
    Utf8StringPooledPtr() noexcept;

    // 拷贝构造：递增 ref_count
    Utf8StringPooledPtr(const Utf8StringPooledPtr& other) noexcept;

    // 移动构造：转移所有权，不改变 ref_count
    Utf8StringPooledPtr(Utf8StringPooledPtr&& other) noexcept;

    // 析构：递减 ref_count，到 0 释放数据
    ~Utf8StringPooledPtr();

    // 拷贝赋值
    Utf8StringPooledPtr& operator=(const Utf8StringPooledPtr& other) noexcept;

    // 移动赋值
    Utf8StringPooledPtr& operator=(Utf8StringPooledPtr&& other) noexcept;

    // ---- 访问 ----

    const u8* data() const noexcept;
    usize     byte_length() const noexcept;
    usize     length() const noexcept;  // 码点个数
    bool      is_empty() const noexcept;

    explicit operator bool() const noexcept;

    // ---- 转换 ----

    // 转为 Utf8StringRef 视图
    Utf8StringRef ref() const noexcept;

    // ---- 比较 ----

    bool operator==(const Utf8StringPooledPtr& other) const noexcept;
    bool operator!=(const Utf8StringPooledPtr& other) const noexcept;

private:
    Utf8PoolEntry* entry_;

    friend class Utf8StringPool;
    explicit Utf8StringPooledPtr(Utf8PoolEntry* entry) noexcept;

    void release() noexcept;
    void acquire() noexcept;
};


// ============================================================================
// Utf8StringPool — 引用计数 UTF-8 字符串池
// ============================================================================

class Utf8StringPool {
public:
    Utf8StringPool() noexcept;
    ~Utf8StringPool();

    // 不可拷贝
    Utf8StringPool(const Utf8StringPool&) = delete;
    Utf8StringPool& operator=(const Utf8StringPool&) = delete;

    // 可移动
    Utf8StringPool(Utf8StringPool&&) noexcept;
    Utf8StringPool& operator=(Utf8StringPool&&) noexcept;

    // ---- intern ----

    // 插入或获取已存在的引用计数条目
    Utf8StringPooledPtr intern(const u8* data, usize byte_length);
    Utf8StringPooledPtr intern(const char* cstr);
    Utf8StringPooledPtr intern(const Utf8StringRef& str);

    // ---- 统计 ----

    // 已分配的 PoolEntry 总数（含墓碑）
    usize size() const noexcept;

    // 活跃条目数（ref_count > 0）
    usize active_entries() const noexcept;

    // 已分配字节总量
    usize total_bytes() const noexcept;

    // 重置，释放全部（调用者需确保无活跃 PooledPtr）
    void clear() noexcept;

private:
    std::list<Utf8PoolEntry> entries_;
    using HashIndex = std::unordered_map<usize, std::vector<Utf8PoolEntry*>>;
    HashIndex hash_index_;

    usize compute_hash(const u8* data, usize byte_length) const noexcept;
};


// ============================================================================
// 非成员比较（对称）
// ============================================================================

bool operator==(const Utf8StringPooledPtr& lhs, const Utf8StringRef& rhs) noexcept;
bool operator!=(const Utf8StringPooledPtr& lhs, const Utf8StringRef& rhs) noexcept;

}  // namespace ca::str


// ============================================================================
// std::hash 特化
// ============================================================================

namespace std {

template <>
struct hash<ca::str::Utf8StringPooledPtr> {
    size_t operator()(const ca::str::Utf8StringPooledPtr& p) const noexcept;
};

}  // namespace std

#endif  // LIBCA_STR_UTF8_STRING_POOL_HPP
