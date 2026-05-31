//
// @brief 不可变 C 风格字符串 (CString)、引用视图 (CStringRef)
//        及可变构建器 (CStringBuilder)
// @author Canrad
// @date 2026/05/31
// @note 命名空间 ca::str，基于 char 类型存储，长度即字符数（O(1)）
//

#ifndef LIBCA_STR_CSTRING_HPP
#define LIBCA_STR_CSTRING_HPP

#include <libca/core/datatype.hpp>

#include <cstddef>
#include <functional>

namespace ca::str {

// ============================================================================
// 前向声明
// ============================================================================

class CString;


// ============================================================================
// CStringRef — 非拥有引用的不可变 char 字符串视图
// ============================================================================

class CStringRef {
public:
    CStringRef() noexcept;
    CStringRef(const char* data, usize length) noexcept;
    CStringRef(const CString& str) noexcept;

    usize length() const noexcept;
    bool isEmpty() const noexcept;
    const char* data() const noexcept;

    char at(usize index) const;
    CStringRef slice(usize start, usize end) const;
    CString substr(usize start, usize count) const;

    int compare(const CStringRef& other) const noexcept;
    bool equals(const CStringRef& other) const noexcept;
    bool operator==(const CStringRef& other) const noexcept;
    bool operator!=(const CStringRef& other) const noexcept;

private:
    const char* data_;
    usize       length_;
};


// ============================================================================
// CString — 拥有所有权的不可变 char 字符串
// ============================================================================

class CString {
public:
    CString() noexcept;
    CString(const char* data, usize length);
    explicit CString(const char* cstr);
    CString(const CString& other);
    CString(CString&& other) noexcept;
    ~CString();

    CString& operator=(const CString& other);
    CString& operator=(CString&& other) noexcept;

    static CString fromCStr(const char* cstr);

    usize length() const noexcept;
    bool isEmpty() const noexcept;
    const char* data() const noexcept;
    const char* cStr() const noexcept;

    char at(usize index) const;
    CStringRef ref() const noexcept;
    CStringRef slice(usize start, usize end) const;
    CString substr(usize start, usize count) const;

    int compare(const CStringRef& other) const noexcept;
    int compare(const CString& other) const noexcept;
    bool equals(const CStringRef& other) const noexcept;
    bool operator==(const CString& other) const noexcept;
    bool operator==(const CStringRef& other) const noexcept;
    bool operator!=(const CString& other) const noexcept;
    bool operator!=(const CStringRef& other) const noexcept;

private:
    char*  data_;
    usize  length_;

    void init(const char* src, usize len);
};


// ============================================================================
// CStringBuilder
// ============================================================================

class CStringBuilder {
public:
    CStringBuilder() noexcept;
    CStringBuilder(const CStringBuilder& other);
    CStringBuilder(CStringBuilder&& other) noexcept;
    ~CStringBuilder();

    CStringBuilder& operator=(const CStringBuilder& other);
    CStringBuilder& operator=(CStringBuilder&& other) noexcept;

    CStringBuilder& append(const CStringRef& str);
    CStringBuilder& append(const CString& str);
    CStringBuilder& append(const char* cstr);
    CStringBuilder& append(const char* data, usize length);
    CStringBuilder& append(char ch);

    void reserve(usize capacity);
    usize capacity() const noexcept;
    usize length() const noexcept;
    bool isEmpty() const noexcept;
    void clear() noexcept;

    CString build() const;

private:
    char*  buffer_;
    usize  length_;
    usize  capacity_;

    static constexpr usize kDefaultCapacity = 64;
    void grow(usize minCapacity);
};


// ============================================================================
// 非成员比较运算符
// ============================================================================

bool operator==(const CStringRef& lhs, const CString& rhs) noexcept;
bool operator!=(const CStringRef& lhs, const CString& rhs) noexcept;

}  // namespace ca::str

namespace std {

template <>
struct hash<ca::str::CString> {
    size_t operator()(const ca::str::CString& s) const noexcept;
};

template <>
struct hash<ca::str::CStringRef> {
    size_t operator()(const ca::str::CStringRef& s) const noexcept;
};

}  // namespace std

#endif  // LIBCA_STR_CSTRING_HPP
