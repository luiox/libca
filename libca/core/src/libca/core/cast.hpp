#pragma once

#include "any.hpp"

#include <type_traits>

namespace ca::core {

// ============================================================================
// Polymorphic — 基类，提供运行时类型识别能力（替代 RTTI）
// 派生类用 CA_TYPE_TAG() 宏实现 type_tag() 即可
// ============================================================================

class Polymorphic {
public:
    virtual const void* type_tag() const noexcept = 0;
    virtual ~Polymorphic() = default;
};

#define CA_TYPE_TAG(T)                                         \
    const void* type_tag() const noexcept override {           \
        return ca::core::type_id<T>();                         \
    }

// ============================================================================
// isa<T>(ptr) — 精确类型检查（匹配 exact dynamic type，非层级）
// ============================================================================

template<typename T, typename U>
bool isa(const U* ptr) noexcept {
    using RawT = std::decay_t<T>;
    return ptr && ptr->type_tag() == type_id<RawT>();
}

// ============================================================================
// cast<T>(ptr) — 无检查向下转型（UB 若类型不匹配）
// ============================================================================

template<typename T, typename U>
auto cast(U* ptr) noexcept -> std::enable_if_t<!std::is_const_v<U>, std::decay_t<T>*> {
    using RawT = std::decay_t<T>;
    return static_cast<RawT*>(ptr);
}

template<typename T, typename U>
auto cast(const U* ptr) noexcept -> const std::decay_t<T>* {
    using RawT = std::decay_t<T>;
    return static_cast<const RawT*>(ptr);
}

// ============================================================================
// dyn_cast<T>(ptr) — 安全向下转型，类型不匹配返回 nullptr
// ============================================================================

template<typename T, typename U>
auto dyn_cast(U* ptr) noexcept -> std::enable_if_t<!std::is_const_v<U>, std::decay_t<T>*> {
    using RawT = std::decay_t<T>;
    return isa<RawT>(ptr) ? static_cast<RawT*>(ptr) : nullptr;
}

template<typename T, typename U>
auto dyn_cast(const U* ptr) noexcept -> const std::decay_t<T>* {
    using RawT = std::decay_t<T>;
    return isa<RawT>(ptr) ? static_cast<const RawT*>(ptr) : nullptr;
}

} // namespace ca::core
