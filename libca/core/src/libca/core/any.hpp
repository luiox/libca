#pragma once

#include "datatype.hpp"

#include <type_traits>
#include <utility>

/// @file any.hpp
/// @brief 不依赖 RTTI 的类型擦除容器 Any，及编译期类型标记 type_id。

namespace ca::core {

/// 每个类型实例化出一个唯一地址，作为该类型的运行时标记（不依赖 typeid）。
/// inline 保证跨翻译单元地址唯一。
template<typename T>
inline const char type_tag_v = 0;

/// @brief 返回类型 T 的唯一标记地址，用于运行时类型识别。
template<typename T>
constexpr const void* type_id() noexcept {
    return &type_tag_v<T>;
}

/// @brief 基于 type_tag 的类型擦除容器，不依赖 RTTI / typeid。
/// @note 可拷贝类型支持完整拷贝/移动；仅移动类型拷贝 Any 后得到空 Any。
///       取值优先用 as<T>()（带类型检查）；cast<T>() 不检查类型，不匹配是 UB。
class Any {
public:
    /// 构造空对象（不持有值）。
    Any() noexcept = default;

    /// 持有 decay_t<T> 的副本/移动值。
    template<typename T, typename = std::enable_if_t<!std::is_same_v<std::decay_t<T>, Any>>>
    Any(T&& value) {
        using Raw = std::decay_t<T>;
        ptr_ = new Raw(std::forward<T>(value));
        tag_ = type_id<Raw>();
        dtor_ = [](void* p) { delete static_cast<Raw*>(p); };
        if constexpr (std::is_copy_constructible_v<Raw>) {
            copy_ = [](const void* p) -> void* { return new Raw(*static_cast<const Raw*>(p)); };
        }
    }

    /// 拷贝构造：可拷贝值会复制；仅移动值复制后为空 Any。
    Any(const Any& other) {
        if (other.ptr_ && other.copy_) {
            ptr_ = other.copy_(other.ptr_);
            tag_ = other.tag_;
            dtor_ = other.dtor_;
            copy_ = other.copy_;
        }
    }

    Any(Any&& other) noexcept { swap(other); }

    Any& operator=(const Any& other) {
        if (this != &other) {
            Any tmp(other);
            swap(tmp);
        }
        return *this;
    }

    Any& operator=(Any&& other) noexcept {
        if (this != &other) {
            reset();
            swap(other);
        }
        return *this;
    }

    ~Any() { reset(); }

    /// 清空，释放持有的值。
    void reset() noexcept {
        if (ptr_ && dtor_) {
            dtor_(ptr_);
            ptr_ = nullptr;
            tag_ = nullptr;
            dtor_ = nullptr;
            copy_ = nullptr;
        }
    }

    /// 是否持有值。
    bool has_value() const noexcept { return ptr_ != nullptr; }

    /// @brief 是否正好持有类型 T（精确匹配，不含继承）。
    template<typename T>
    bool is() const noexcept {
        return tag_ == type_id<std::decay_t<T>>();
    }

    /// @brief 类型匹配则返回指针，否则返回 nullptr。取值的安全方式。
    template<typename T>
    std::decay_t<T>* as() noexcept {
        if (tag_ != type_id<std::decay_t<T>>()) return nullptr;
        return static_cast<std::decay_t<T>*>(ptr_);
    }

    /// @copydoc as()
    template<typename T>
    const std::decay_t<T>* as() const noexcept {
        if (tag_ != type_id<std::decay_t<T>>()) return nullptr;
        return static_cast<const std::decay_t<T>*>(ptr_);
    }

    /// @brief 无检查取引用。@warning 类型不匹配是未定义行为，不确定时用 as<T>()。
    template<typename T>
    std::decay_t<T>& cast() {
        return *static_cast<std::decay_t<T>*>(ptr_);
    }

    /// @copydoc cast()
    template<typename T>
    const std::decay_t<T>& cast() const {
        return *static_cast<const std::decay_t<T>*>(ptr_);
    }

    /// 当前持有类型的标记地址；空对象为 nullptr。
    const void* type_tag() const noexcept { return tag_; }

private:
    void swap(Any& other) noexcept {
        using std::swap;
        swap(ptr_, other.ptr_);
        swap(tag_, other.tag_);
        swap(dtor_, other.dtor_);
        swap(copy_, other.copy_);
    }

    void* ptr_ = nullptr;
    const void* tag_ = nullptr;
    void (*dtor_)(void*) = nullptr;
    void* (*copy_)(const void*) = nullptr;
};

} // namespace ca::core
