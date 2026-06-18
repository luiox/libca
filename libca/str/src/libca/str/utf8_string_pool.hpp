/// @file utf8_string_pool.hpp
/// @brief 引用计数 UTF-8 字符串池 Utf8StringPool 及池化句柄 Utf8StringPooledPtr。
/// @author Canrad
/// @date 2026/05/31
/// @note **Provisional**：接口形状已稳定，但 disown fail-safe 语义较新，下游充分验证前
///       勿写入自身公共稳定接口。**非线程安全**。
/// @note 选型：vs Utf8StringArena——arena 是"同生共死、整块释放"，pool 是"各句柄寿命不同、
///       按引用计数回收"。一批字符串有共同死亡点用 arena，无则用 pool。
/// @note 生命周期契约（核心）：
///       - **真删**：PooledPtr refcount 归零即 delete 该 entry（非墓碑）。
///       - **outlive 是软契约（性能契约，非硬约束）**：Pool 先于 PooledPtr 析构 / clear() /
///         move-assign 时**不会 UAF**（走 disown fail-safe，存活句柄自管释放），但被 disown 的
///         entry 失去去重收益。遵守 outlive 则享真删+去重，违反则退化自管。
///       - **Ref 仍由借用纪律保证**：PooledPtr 析构后，由它 .ref() 派生的 Utf8StringRef 失效。
/// @code
///   Utf8StringPool pool;
///   auto s1 = pool.intern("hello");  // refcount=1
///   auto s2 = s1;                    // refcount=2；s2、s1 依次析构后 entry 真删
/// @endcode

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


/// @brief 池内条目：数据 + 引用计数 + 回指 owner（refcount 归零时由 owner 真删）。
struct Utf8PoolEntry {
    u8*             data;
    usize           byte_length;
    usize           length;    // 码点个数
    usize           hash;
    usize           ref_count;
    Utf8StringPool* owner;     // 回指：ref_count 归零时由它真删本条目；被 disown 后为 nullptr
};


/// @brief 引用计数的池化不可变 UTF-8 字符串句柄，8 字节（一个指针）。
/// @note 拷贝/赋值 refcount++，析构 refcount--，归零触发真删或（被 disown 时）自管释放。
class Utf8StringPooledPtr {
public:
    /// 空句柄。
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

    /// 转为 Utf8StringRef 视图（借用纪律同 Ref：PooledPtr 须存活）。
    Utf8StringRef ref() const noexcept;

    /// @brief 隐式转 Utf8StringRef（零开销借用降级），可直接喂给只读接口。
    /// @warning 只读期间 PooledPtr 须存活（拥有者 outlive 借用者）。
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

/// @brief 引用计数 UTF-8 字符串池。详见文件头的选型与生命周期契约。
class Utf8StringPool {
public:
    Utf8StringPool() noexcept;
    /// 析构：disown_all fail-safe 释放（残留句柄转为自管，不 UAF）。
    ~Utf8StringPool();

    Utf8StringPool(const Utf8StringPool&) = delete;
    Utf8StringPool& operator=(const Utf8StringPool&) = delete;

    Utf8StringPool(Utf8StringPool&&) noexcept;
    Utf8StringPool& operator=(Utf8StringPool&&) noexcept;

    // ---- intern：复制入池、去重，返回引用计数句柄 ----

    /// @brief 校验 UTF-8 并复制入池、去重。空/非法返回空句柄。
    Utf8StringPooledPtr intern(const u8* data, usize byte_length);
    /// intern C 字符串；空指针返回空句柄。
    Utf8StringPooledPtr intern(const char* cstr);
    /// intern 视图内容。
    Utf8StringPooledPtr intern(const Utf8StringRef& str);

    // ---- 查找（不分配、不改 ref_count）----

    /// @brief 按内容查已存在条目。命中返回持有该条目的句柄（refcount++），未命中返回空句柄。
    /// @note 替代 C++20 才有的异构查找（C++17 不可用）。
    Utf8StringPooledPtr find(const Utf8StringRef& str) const;

    // ---- 统计 ----

    usize size() const noexcept;            ///< 唯一条目数（真删后 == 活条目数）
    usize active_entries() const noexcept;  ///< 活跃条目数（ref_count > 0）
    usize total_bytes() const noexcept;     ///< 活条目字节和

    /// disown_all fail-safe，回到空池（残留句柄转自管，不 UAF）。
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
