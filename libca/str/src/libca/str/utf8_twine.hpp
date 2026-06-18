/// @file utf8_twine.hpp
/// @brief 惰性 UTF-8 字符串拼接 Utf8Twine（LLVM Twine 风格二叉拼接树，拼接零分配）。
/// @author Canrad
/// @note **借用语义同 Utf8StringRef**：只在单表达式内作参数使用，绝不存成员、绝不跨语句。
///       仅在 materialize()/to_string() 时一次性产出。
///       叶子按值存 Utf8StringRef（字节由调用方保证存活）；子 Twine 按指针存（指向表达式内栈临时量）。
/// @code
///   pool.intern(a + "/" + b);          // → PooledPtr
///   arena.intern(Utf8Twine(x) + y);    // → Utf8StringRef
///   (a + b).to_string();               // → Utf8String（独立拥有）
/// @endcode

#pragma once

#include "libca/core/datatype.hpp"
#include "utf8_string.hpp"

namespace ca::str {

class Utf8StringArena;
class Utf8StringPool;
class Utf8StringPooledPtr;

/// @brief 惰性拼接节点。详见文件头借用语义。
class Utf8Twine {
public:
    // ---- 叶子构造（隐式，让 "a" + ref 这类表达式成立）----
    Utf8Twine() noexcept;                          // 空
    Utf8Twine(const char* cstr) noexcept;          // C 字面量
    Utf8Twine(const Utf8StringRef& ref) noexcept;  // 视图（PooledPtr/ZUtf8StringRef 经隐式转换走这里）
    Utf8Twine(const Utf8String& s) noexcept;       // 取其视图

    // ---- 拼接 ----
    Utf8Twine concat(const Utf8Twine& rhs) const noexcept;

    // ---- 查询 ----
    bool is_empty() const noexcept;
    usize byte_length() const noexcept;            // 各片段字节和（O(片段数)）

    // ---- 一次性产出 ----
    Utf8String          to_string() const;                 // 独立拥有者
    Utf8StringRef       materialize(Utf8StringArena& arena) const;   // intern 进 arena
    Utf8StringPooledPtr materialize(Utf8StringPool& pool) const;     // intern 进 pool

private:
    enum class Kind : u8 { Empty, Ref, Twine };
    struct Child {
        Utf8StringRef    ref;     // Kind::Ref 时有效（按值）
        const Utf8Twine* twine;   // Kind::Twine 时有效（按指针，指向栈临时量）
    };
    Child lhs_{};
    Child rhs_{};
    Kind  lhsKind_ = Kind::Empty;
    Kind  rhsKind_ = Kind::Empty;

    Utf8Twine(const Utf8Twine& l, const Utf8Twine& r) noexcept;  // 二元拼接节点
    void append_to(Utf8StringBuilder& b) const;                  // 递归展开到 builder
    void append_child(Utf8StringBuilder& b, const Child& c, Kind k) const;
    usize child_bytes(const Child& c, Kind k) const noexcept;
};

// a + b（两侧经隐式转换为 Utf8Twine）
inline Utf8Twine operator+(const Utf8Twine& l, const Utf8Twine& r) noexcept {
    return l.concat(r);
}

}  // namespace ca::str
