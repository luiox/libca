//
// @brief 不可变宽字符串 (WString)、引用视图 (WStringRef)
//        及可变构建器 (WStringBuilder)
// @author Canrad
// @date 2026/05/31
// @note 命名空间 ca::str，基于 wchar_t 类型存储
//

#ifndef LIBCA_STR_WSTRING_HPP
#define LIBCA_STR_WSTRING_HPP

#include <libca/core/datatype.hpp>

#include <cstddef>
#include <functional>

namespace ca::str {

class WString;

class WStringRef {
public:
    WStringRef() noexcept;
    WStringRef(const wchar_t* data, usize length) noexcept;
    WStringRef(const WString& str) noexcept;

    usize length() const noexcept;
    bool isEmpty() const noexcept;
    const wchar_t* data() const noexcept;
    wchar_t at(usize index) const;

    WStringRef slice(usize start, usize end) const;
    WString substr(usize start, usize count) const;

    int compare(const WStringRef& other) const noexcept;
    bool equals(const WStringRef& other) const noexcept;
    bool operator==(const WStringRef& other) const noexcept;
    bool operator!=(const WStringRef& other) const noexcept;

private:
    const wchar_t* data_;
    usize          length_;
};

class WString {
public:
    WString() noexcept;
    WString(const wchar_t* data, usize length);
    explicit WString(const wchar_t* wstr);
    WString(WString&& other) noexcept;
    ~WString();

    WString& operator=(WString&& other) noexcept;

    // 显式克隆（唯一复制方式）
    WString clone() const;

    static WString fromWStr(const wchar_t* wstr);

    usize length() const noexcept;
    bool isEmpty() const noexcept;
    const wchar_t* data() const noexcept;
    const wchar_t* wStr() const noexcept;
    wchar_t at(usize index) const;

    WStringRef ref() const noexcept;
    WStringRef slice(usize start, usize end) const;
    WString substr(usize start, usize count) const;

    int compare(const WStringRef& other) const noexcept;
    int compare(const WString& other) const noexcept;
    bool equals(const WStringRef& other) const noexcept;
    bool operator==(const WString& other) const noexcept;
    bool operator==(const WStringRef& other) const noexcept;
    bool operator!=(const WString& other) const noexcept;
    bool operator!=(const WStringRef& other) const noexcept;

private:
    wchar_t* data_;
    usize    length_;
    void init(const wchar_t* src, usize len);
};

class WStringBuilder {
public:
    WStringBuilder() noexcept;
    WStringBuilder(WStringBuilder&& other) noexcept;
    ~WStringBuilder();

    WStringBuilder& operator=(WStringBuilder&& other) noexcept;

    // 拷贝 = delete
    WStringBuilder(const WStringBuilder&) = delete;
    WStringBuilder& operator=(const WStringBuilder&) = delete;

    WStringBuilder& append(const WStringRef& str);
    WStringBuilder& append(const WString& str);
    WStringBuilder& append(const wchar_t* wstr);
    WStringBuilder& append(const wchar_t* data, usize length);
    WStringBuilder& append(wchar_t ch);

    void reserve(usize capacity);
    usize capacity() const noexcept;
    usize length() const noexcept;
    bool isEmpty() const noexcept;
    void clear() noexcept;

    WString build() const;

private:
    wchar_t* buffer_;
    usize    length_;
    usize    capacity_;
    static constexpr usize kDefaultCapacity = 64;
    void grow(usize minCapacity);
};

bool operator==(const WStringRef& lhs, const WString& rhs) noexcept;
bool operator!=(const WStringRef& lhs, const WString& rhs) noexcept;

}  // namespace ca::str

namespace std {

template <>
struct hash<ca::str::WString> {
    size_t operator()(const ca::str::WString& s) const noexcept;
};

template <>
struct hash<ca::str::WStringRef> {
    size_t operator()(const ca::str::WStringRef& s) const noexcept;
};

}  // namespace std

#endif  // LIBCA_STR_WSTRING_HPP
