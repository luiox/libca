#ifndef LIBCA_REFLECT_ENUM_HPP
#define LIBCA_REFLECT_ENUM_HPP

#include <string>
#include <stdexcept>
#include <vector>
#include <unordered_map>
#include <memory>
#include "libca/base/Result.hpp"

namespace ca {

template<class T, T N>

const char* get_enum_name_static()
{
#if defined(_MSC_VER)
    return __FUNCSIG__;
#else
    return __PRETTY_FUNCTION__;
#endif
}

template<bool Cond>
struct my_enable_if
{};

template<>
struct my_enable_if<true>
{
    typedef void type;
};

template<int Beg, int End, class F>
typename my_enable_if<Beg == End>::type static_for(F const& func)
{}

template<int Beg, int End, class F>
typename my_enable_if<Beg != End>::type static_for(F const& func)
{
    func.template call<Beg>();
    static_for<Beg + 1, End>(func);
}

template<class T>
struct get_enum_name_functor
{
    int          n;
    std::string& s;

    get_enum_name_functor(int n, std::string& s)
        : n(n)
        , s(s)
    {}

    template<int I>
    void call() const
    {
        if (n == I)
            s = get_enum_name_static<T, (T)I>();
    }
};


template<class T, T Beg, T End>
std::string get_enum_name(T n)
{
    std::string s;
    ca::static_for<Beg, End + 1>(ca::get_enum_name_functor<T>(n, s));
    if (s.empty())
        return "";
#if defined(_MSC_VER)
    size_t pos = s.find(',');
    pos += 1;
    size_t pos2 = s.find('>', pos);
#else
    size_t pos = s.find("N = ");
    pos += 4;
    size_t pos2 = s.find_first_of(";]", pos);
#endif
    s           = s.substr(pos, pos2 - pos);
    size_t pos3 = s.find("::");
    if (pos3 != s.npos)
        s = s.substr(pos3 + 2);
    return s;
}

template<class T>
std::string get_enum_name(T n)
{
    return get_enum_name<T, (T)0, (T)256>(n);
}

template<class T, T Beg, T End>
T enum_from_name(std::string const& s)
{
    for (int i = (int)Beg; i < (int)End; i++) {
        if (s == get_enum_name((T)i)) {
            return (T)i;
        }
    }
    throw std::runtime_error("Unknown enum name: " + s);
}

template<class T>
T enum_from_name(std::string const& s)
{
    return enum_from_name<T, (T)0, (T)256>(s);
}


// 计算类成员的个数
struct AnyHelper
{
    template<typename T>
    operator T();   // 无定义 我们需要一个可以转换为任何类型的转换类
};
template<unsigned I>
struct tag : tag<I - 1>
{};
template<>
struct tag<0>
{};

template<typename T>
constexpr auto size_(tag<4>) -> decltype(T{AnyHelper{}, AnyHelper{}, AnyHelper{}, AnyHelper{}}, 0u)
{
    return 4u;
}
template<typename T>
constexpr auto size_(tag<3>) -> decltype(T{AnyHelper{}, AnyHelper{}, AnyHelper{}}, 0u)
{
    return 3u;
}
template<typename T>
constexpr auto size_(tag<2>) -> decltype(T{AnyHelper{}, AnyHelper{}}, 0u)
{
    return 2u;
}
template<typename T>
constexpr auto size_(tag<1>) -> decltype(T{AnyHelper{}}, 0u)
{
    return 1u;
}
template<typename T>
constexpr auto size_(tag<0>) -> decltype(T{}, 0u)
{
    return 0u;
}
template<typename T>
constexpr size_t size()
{
    static_assert(std::is_aggregate_v<T>);
    return size_<T>(tag<4>{});
}

template<typename T, typename F>
void for_each_member(T const& v, F&& f)
{
    static_assert(std::is_aggregate_v<T>);

    if constexpr (size<T>() == 4u) {
        const auto& [m0, m1, m2, m3] = v;
        f(m0);
        f(m1);
        f(m2);
        f(m3);
    }
    else if constexpr (size<T>() == 3u) {
        const auto& [m0, m1, m2] = v;
        f(m0);
        f(m1);
        f(m2);
    }
    else if constexpr (size<T>() == 2u) {
        const auto& [m0, m1] = v;
        f(m0);
        f(m1);
    }
    else if constexpr (size<T>() == 1u) {
        const auto& [m0] = v;
        f(m0);
    }
}





}   // namespace ca


namespace detail {

template<typename Fn, typename Tuple, std::size_t... I>
inline constexpr void ForEachTuple(Tuple&& tuple, Fn&& fn, std::index_sequence<I...>)
{
    using Expander = int[];
    (void)Expander{0, ((void)fn(std::get<I>(std::forward<Tuple>(tuple))), 0)...};
}

template<typename Fn, typename Tuple>
inline constexpr void ForEachTuple(Tuple&& tuple, Fn&& fn)
{
    ForEachTuple(std::forward<Tuple>(tuple),
                 std::forward<Fn>(fn),
                 std::make_index_sequence<std::tuple_size<std::decay_t<Tuple>>::value>{});
}

template<typename T>
struct is_field_pointer : std::false_type
{};

template<typename C, typename T>
struct is_field_pointer<T C::*> : std::true_type
{};

template<typename T>
constexpr auto is_field_pointer_v = is_field_pointer<T>::value;

}   // namespace detail

template<typename T>
inline constexpr auto StructSchema()
{
    return std::make_tuple();
}

#define DEFINE_STRUCT_SCHEMA(Struct, ...)        \
    template<>                                   \
    inline constexpr auto StructSchema<Struct>() \
    {                                            \
        using _Struct = Struct;                  \
        return std::make_tuple(__VA_ARGS__);     \
    }

#define DEFINE_STRUCT_FIELD(StructField, FieldName) \
    std::make_tuple(&_Struct::StructField, FieldName)

template<typename T, typename Fn>
inline constexpr void ForEachField(T&& value, Fn&& fn)
{
    constexpr auto struct_schema = StructSchema<std::decay_t<T>>();
    static_assert(std::tuple_size<decltype(struct_schema)>::value != 0,
                  "StructSchema<T>() for type T should be specialized to return "
                  "FieldSchema tuples, like ((&T::field, field_name), ...)");

    detail::ForEachTuple(struct_schema, [&value, &fn](auto&& field_schema) {
        using FieldSchema = std::decay_t<decltype(field_schema)>;
        static_assert(std::tuple_size<FieldSchema>::value >= 2 &&
                          detail::is_field_pointer_v<std::tuple_element_t<0, FieldSchema>>,
                      "FieldSchema tuple should be (&T::field, field_name)");

        fn(value.*(std::get<0>(std::forward<decltype(field_schema)>(field_schema))),
           std::get<1>(std::forward<decltype(field_schema)>(field_schema)));
    });
}
namespace ca {

class FieldInfo
{
public:
    std::string name_;
    std::string type_;
    size_t offset_;

    FieldInfo(std::string name, std::string type, size_t offset)
        : name_(name)
        , type_(type)
        , offset_(offset)
    {}
};

class ParamInfo
{
public:
    std::string name_;
    std::string type_;

    ParamInfo(std::string name, std::string type)
        : name_(name)
        , type_(type)
    {}
};

class MethodInfo
{
public:
    std::string            name_;
    std::string            returnType_;
    std::vector<ParamInfo> args_;
};

class ClassInfo
{
public:
    std::string             name_;
    std::vector<MethodInfo> methods_;
    std::vector<FieldInfo>  fields_;
};

class RuntimeInfoManager
{
private:
    std::unordered_map<std::string, ClassInfo> classInfos_;

public:
    void registerClass(const std::string& className, const ClassInfo& classInfo)
    {
        classInfos_[className] = classInfo;
    }

    Result<ClassInfo, std::string> getClassInfo(const std::string& className) const
    {
        if (classInfos_.find(className) != classInfos_.end()) {
            return Err(std::string("Class not found: "));
        }
        return Ok(classInfos_.at(className));
    }
};



}   // namespace ca

#endif   // LIBCA_REFLECT_ENUM_HPP
