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

#pragma once

#include "libca/core/datatype.hpp"
#include "utf8_string.hpp"

#include <cstddef>
#include <cstring>
#include <unordered_map>
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
    u8*             data;
    usize           byte_length;
    usize           length;    // 码点个数
    usize           hash;
    usize           ref_count;
    Utf8StringPool* owner;     // 回指：ref_count 归零时由它真删本条目
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

    // 隐式转为 Utf8StringRef（零开销借用降级，对齐 ZUtf8StringRef）
    // 让 PooledPtr 直接喂给只读接口；只读期间 PooledPtr 须存活（拥有者 outlive 借用者）
    operator Utf8StringRef() const noexcept { return ref(); }

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

    // ---- 查找（不分配、不改 ref_count）----

    // 按内容查已存在条目。命中返回持有该条目的 PooledPtr（ref_count++），
    // 未命中返回空 PooledPtr。Pool 本身即「内容 → PooledPtr」索引——
    // 替代 C++20 才有的 unordered_map 异构查找（C++17 不可用）。
    Utf8StringPooledPtr find(const Utf8StringRef& str) const;

    // ---- 统计 ----

    // 唯一条目总数（真删后 == 活条目数）
    usize size() const noexcept;

    // 活跃条目数（ref_count > 0）
    usize active_entries() const noexcept;

    // 已分配字节总量
    usize total_bytes() const noexcept;

    // 重置，释放全部（调用者需确保无活跃 PooledPtr）
    void clear() noexcept;

private:
    // hash_index_ 是条目的唯一所有者：hash → 同 hash 的堆分配条目列表（冲突链）。
    // 条目堆分配 → 指针稳定，PooledPtr 可安全长持。ref_count 归零即真删（无墓碑）。
    using HashIndex = std::unordered_map<usize, std::vector<Utf8PoolEntry*>>;
    HashIndex hash_index_;
    usize     active_count_ = 0;   // 活条目数（O(1)）
    usize     total_bytes_  = 0;   // 活条目字节和（O(1)）

    usize compute_hash(const u8* data, usize byte_length) const noexcept;

    // 真删：从 hash 桶摘除 entry、释放其字节、delete 条目、空桶则删 key。
    // 由 PooledPtr::release() 在 ref_count 归零时回调。
    void erase_entry(Utf8PoolEntry* entry) noexcept;
    // move 后把所有条目的 owner 回指改向 this
    void repoint_entries() noexcept;
    // Pool 退出（析构/clear/move-assign 释放自身旧条目）前的 fail-safe：
    // hash_index_ 里残留的 entry 都仍有外部 PooledPtr 持有（ref_count 归零的早已被
    // erase_entry 真删移出），一律置 owner=nullptr，由存活句柄自管释放。
    // 消除「Pool 先死、PooledPtr 后死」UAF。
    void disown_all() noexcept;
    friend class Utf8StringPooledPtr;
};


// ============================================================================
// 非成员比较（对称）
// ============================================================================

bool operator==(const Utf8StringPooledPtr& lhs, const Utf8StringRef& rhs) noexcept;
bool operator!=(const Utf8StringPooledPtr& lhs, const Utf8StringRef& rhs) noexcept;
// 反向对称：Ref == PooledPtr（内容比较）
bool operator==(const Utf8StringRef& lhs, const Utf8StringPooledPtr& rhs) noexcept;
bool operator!=(const Utf8StringRef& lhs, const Utf8StringPooledPtr& rhs) noexcept;

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
