#pragma once

#include "any.hpp"

#include <type_traits>

/// @file cast.hpp
/// @brief 不依赖 RTTI 的运行时类型识别与向下转型：Polymorphic 基类 + isa/cast/dyn_cast。
/// @note 类型匹配是**精确动态类型**匹配，不是"是否为某基类"。实际对象是 Human 时，
///       isa<Mammal> 返回 false、isa<Human> 返回 true。

namespace ca::core {

/// @brief 提供运行时类型识别能力的基类（替代 RTTI）。派生类用 CA_TYPE_TAG(T) 实现。
class Polymorphic {
public:
    virtual const void* type_tag() const noexcept = 0;
    virtual ~Polymorphic() = default;
};

/// 在派生类体内展开，实现 type_tag()。用法：`CA_TYPE_TAG(MyType)`。
#define CA_TYPE_TAG(T)                                         \
    const void* type_tag() const noexcept override {           \
        return ca::core::type_id<T>();                         \
    }

/// @brief 精确类型检查（匹配 exact dynamic type，非层级）。空指针返回 false。
template<typename T, typename U>
bool isa(const U* ptr) noexcept {
    using RawT = std::decay_t<T>;
    return ptr && ptr->type_tag() == type_id<RawT>();
}

/// @brief 无检查向下转型，等价于确认类型后的 static_cast。
/// @warning 类型不匹配是未定义行为；不确定时用 dyn_cast<T>()。
template<typename T, typename U>
auto cast(U* ptr) noexcept -> std::enable_if_t<!std::is_const_v<U>, std::decay_t<T>*> {
    using RawT = std::decay_t<T>;
    return static_cast<RawT*>(ptr);
}

/// @copydoc cast()
template<typename T, typename U>
auto cast(const U* ptr) noexcept -> const std::decay_t<T>* {
    using RawT = std::decay_t<T>;
    return static_cast<const RawT*>(ptr);
}

/// @brief 安全向下转型：精确类型匹配则返回转换后指针，否则返回 nullptr。
template<typename T, typename U>
auto dyn_cast(U* ptr) noexcept -> std::enable_if_t<!std::is_const_v<U>, std::decay_t<T>*> {
    using RawT = std::decay_t<T>;
    return isa<RawT>(ptr) ? static_cast<RawT*>(ptr) : nullptr;
}

/// @copydoc dyn_cast()
template<typename T, typename U>
auto dyn_cast(const U* ptr) noexcept -> const std::decay_t<T>* {
    using RawT = std::decay_t<T>;
    return isa<RawT>(ptr) ? static_cast<const RawT*>(ptr) : nullptr;
}

} // namespace ca::core
