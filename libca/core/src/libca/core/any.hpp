#pragma once

#include "datatype.hpp"

#include <type_traits>
#include <utility>

namespace ca::core {

// ============================================================================
// type_tag_v<T> — 每实例化一个类型产生一个唯一地址，用于运行时类型识别
// inline 确保跨 TU 地址唯一
// ============================================================================

template<typename T>
inline const char type_tag_v = 0;

template<typename T>
constexpr const void* type_id() noexcept {
    return &type_tag_v<T>;
}

// ============================================================================
// Any — 基于 type_tag 的类型擦除容器，不依赖 RTTI / typeid
//
// 类型要求:
//   - 可拷贝类型: 支持完整的拷贝/移动语义
//   - 仅移动类型: 仅支持移动构造/赋值，拷贝 Any 将得到空 Any
// ============================================================================

class Any {
public:
    Any() noexcept = default;

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

    void reset() noexcept {
        if (ptr_ && dtor_) {
            dtor_(ptr_);
            ptr_ = nullptr;
            tag_ = nullptr;
            dtor_ = nullptr;
            copy_ = nullptr;
        }
    }

    bool has_value() const noexcept { return ptr_ != nullptr; }

    template<typename T>
    bool is() const noexcept {
        return tag_ == type_id<std::decay_t<T>>();
    }

    template<typename T>
    std::decay_t<T>* as() noexcept {
        if (tag_ != type_id<std::decay_t<T>>()) return nullptr;
        return static_cast<std::decay_t<T>*>(ptr_);
    }

    template<typename T>
    const std::decay_t<T>* as() const noexcept {
        if (tag_ != type_id<std::decay_t<T>>()) return nullptr;
        return static_cast<const std::decay_t<T>*>(ptr_);
    }

    template<typename T>
    std::decay_t<T>& cast() {
        return *static_cast<std::decay_t<T>*>(ptr_);
    }

    template<typename T>
    const std::decay_t<T>& cast() const {
        return *static_cast<const std::decay_t<T>*>(ptr_);
    }

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
