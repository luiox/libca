#pragma once

#include <type_traits>

/// @file tag_cast.hpp
/// @brief 面向整数 tag 层级的 RTTI-free 向下转型：LLVM 风格 isa/cast/dyn_cast。
///
/// 适用于"kind 即域数据"的类层级——每个节点本就携带 int 型 getType() 标签
/// （序列化/分派要用），转型零额外成本。类型经特化 TypeOf<T> 映射到其标签值
/// 后即可参与识别；与 Polymorphic（cast.hpp，编译期 type_id 自动打标、精确
/// 动态类型匹配）并存，语义不同：
/// - Polymorphic：任意多态类型的通用识别，需继承基类（侵入）；
/// - tag_cast：既有 kind 层级的零成本向下转型（非侵入，不要求共同基类提供
///   type_tag()）。
///
/// 全部函数不抛异常。

namespace ca::core::tag_cast {

/// @brief 把具体类型映射到其整数类型标签。
///
/// 为层级中每个叶子类型特化：
/// `template<> struct TypeOf<Foo> { static constexpr int value = FOO_TAG; };`
/// 该值必须等于运行期 `Foo::getType()` 的返回值。isa/dyn_cast 依赖此特化；
/// 无检查的 cast 不查询它。
///
/// @note `const T` 形式的目标类型自动解析到 `T` 特化。
template<typename T>
struct TypeOf;

template<typename T>
struct TypeOf<const T> : TypeOf<T>
{};

namespace detail {

template<typename T>
struct remove_star
{
    using type = T;
};
template<typename T>
struct remove_star<T*>
{
    using type = T;
};

/// 把调用方给定的目标 T（Foo / Foo* / const Foo 皆可）归一到裸类类型，
/// 使 cast<Foo> 与 cast<Foo*> 行为一致。
template<typename T>
using cleanup_t = std::remove_cv_t<typename remove_star<T>::type>;

}   // namespace detail

/// @brief 无检查向下转型（static_cast）到目标类型。
///
/// @warning 不做任何类型检查；ptr 实际不是目标实例时是未定义行为——不确定时
///          先用 isa 守护或改用 dyn_cast。
/// @return 以目标指针形态返回 ptr；保持实参 const 性；ptr 为空返回空。
template<typename T, typename U>
auto cast(U* ptr)
{
    using RawT = detail::cleanup_t<T>;
    return static_cast<RawT*>(ptr);
}

/// @copydoc cast()
template<typename T, typename U>
auto cast(const U* ptr) -> const std::decay_t<detail::cleanup_t<T>>*
{
    using RawT = detail::cleanup_t<T>;
    return static_cast<const RawT*>(ptr);
}

/// @brief 按 tag 判型：比较 ptr->getType() 与 TypeOf<T>::value。
///
/// @return ptr 为空返回 false；否则返回 tag 比较结果。
/// @note 仅提供 const 指针形参重载，可同时接受 const 与非 const 实参。
template<typename T, typename U>
bool isa(const U* ptr)
{
    using RawT = detail::cleanup_t<T>;
    return ptr && ptr->getType() == TypeOf<RawT>::value;
}

/// @brief 受检向下转型：isa 成立等价 cast，否则返回 nullptr。
///
/// @return tag 匹配时返回转换后指针，否则 nullptr（含空实参）。
/// @note 仅提供 const 指针重载，结果恒为 const 目标指针；需要可变指针时用
///       isa 守护后走 cast。
template<typename T, typename U>
auto dyn_cast(const U* ptr) -> decltype(cast<T>(ptr))
{
    return isa<T>(ptr) ? cast<T>(ptr) : nullptr;
}

}   // namespace ca::core::tag_cast
