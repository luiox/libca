//
// @brief Utf8Twine 实现
//

#include "libca/str/utf8_twine.hpp"

#include "libca/str/utf8_string_arena.hpp"
#include "libca/str/utf8_string_pool.hpp"

namespace ca::str {

// ---- 叶子构造 ----

Utf8Twine::Utf8Twine() noexcept = default;

Utf8Twine::Utf8Twine(const char* cstr) noexcept {
    lhs_.ref = Utf8StringRef::from_cstr(cstr);
    lhsKind_ = Kind::Ref;
}

Utf8Twine::Utf8Twine(const Utf8StringRef& ref) noexcept {
    lhs_.ref = ref;
    lhsKind_ = Kind::Ref;
}

Utf8Twine::Utf8Twine(const Utf8String& s) noexcept {
    lhs_.ref = s.ref();
    lhsKind_ = Kind::Ref;
}

// ---- 二元拼接节点 ----

Utf8Twine::Utf8Twine(const Utf8Twine& l, const Utf8Twine& r) noexcept {
    lhs_.twine = &l;
    lhsKind_   = Kind::Twine;
    rhs_.twine = &r;
    rhsKind_   = Kind::Twine;
}

Utf8Twine Utf8Twine::concat(const Utf8Twine& rhs) const noexcept {
    return Utf8Twine(*this, rhs);
}

// ---- 查询 ----

bool Utf8Twine::is_empty() const noexcept {
    return byte_length() == 0;
}

usize Utf8Twine::child_bytes(const Child& c, Kind k) const noexcept {
    switch (k) {
        case Kind::Ref:   return c.ref.byte_length();
        case Kind::Twine: return c.twine ? c.twine->byte_length() : 0;
        case Kind::Empty: return 0;
    }
    return 0;
}

usize Utf8Twine::byte_length() const noexcept {
    return child_bytes(lhs_, lhsKind_) + child_bytes(rhs_, rhsKind_);
}

// ---- 展开 ----

void Utf8Twine::append_child(Utf8StringBuilder& b, const Child& c, Kind k) const {
    switch (k) {
        case Kind::Ref:   if (!c.ref.is_empty()) b.append(c.ref); break;
        case Kind::Twine: if (c.twine) c.twine->append_to(b);     break;
        case Kind::Empty: break;
    }
}

void Utf8Twine::append_to(Utf8StringBuilder& b) const {
    append_child(b, lhs_, lhsKind_);
    append_child(b, rhs_, rhsKind_);
}

// ---- 产出 ----

Utf8String Utf8Twine::to_string() const {
    Utf8StringBuilder b;
    b.reserve(byte_length());
    append_to(b);
    return b.build_or_empty();
}

Utf8StringRef Utf8Twine::materialize(Utf8StringArena& arena) const {
    // 单一叶子且无右子：直接 intern 该视图，省一次 builder 拷贝
    if (lhsKind_ == Kind::Ref && rhsKind_ == Kind::Empty) {
        return arena.intern(lhs_.ref);
    }
    Utf8String s = to_string();
    return arena.intern(s);
}

Utf8StringPooledPtr Utf8Twine::materialize(Utf8StringPool& pool) const {
    if (lhsKind_ == Kind::Ref && rhsKind_ == Kind::Empty) {
        return pool.intern(lhs_.ref);
    }
    Utf8String s = to_string();
    return pool.intern(s.ref());
}

}  // namespace ca::str
