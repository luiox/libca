/// @file cstring.hpp
/// @brief 不可变 char 字符串：拥有所有权的 CString、非拥有视图 CStringRef、可变构建器 CStringBuilder。
/// @author Canrad
/// @date 2026/05/31
/// @note 命名空间 ca::str，按 char 存储，length() = 字符数（O(1)），不做 UTF-8 码点语义。
///       需要 Unicode 码点处理用 Utf8String；本类型用于纯 char/字节级字符串。

#pragma once

#include "libca/core/datatype.hpp"

#include <cstddef>
#include <functional>
#include <string>
#include <vector>

namespace ca::str {

// ============================================================================
// 前向声明
// ============================================================================

class CString;


/// @brief 非拥有、不可变的 char 字符串视图。
/// @warning 不拥有数据；来自 CString::ref()/slice() 的视图在原串销毁或移动后失效。
class CStringRef {
public:
    CStringRef() noexcept;
    CStringRef(const char* data, usize length) noexcept;
    CStringRef(const CString& str) noexcept;

    usize length() const noexcept;
    bool is_empty() const noexcept;
    const char* data() const noexcept;

    char at(usize index) const;
    CStringRef slice(usize start, usize end) const;
    CString substr(usize start, usize count) const;

    // ---- 前缀/后缀 ----
    bool starts_with(const CStringRef& prefix) const noexcept;
    bool ends_with(const CStringRef& suffix) const noexcept;

    // ---- 修剪 ----
    CStringRef trim() const noexcept;
    CStringRef trim_start() const noexcept;
    CStringRef trim_end() const noexcept;

    // ---- 拆分 ----
    std::vector<CStringRef> split(const CStringRef& delimiter) const;

    // ---- 大小写转换 ----
    CString to_lower() const;
    CString to_upper() const;

    // ---- 替换 ----
    CString replace_all(const CStringRef& from, const CStringRef& to) const;

    // ---- 比较 ----
    int compare(const CStringRef& other) const noexcept;
    bool equals(const CStringRef& other) const noexcept;
    bool operator==(const CStringRef& other) const noexcept;
    bool operator!=(const CStringRef& other) const noexcept;

private:
    const char* data_;
    usize       length_;
};


/// @brief 拥有所有权、不可变、内部以 `\0` 终止的 char 字符串。
/// @note 禁止隐式拷贝（须显式 clone()），可移动。保证结尾 `\0`，故提供 c_str()。
class CString {
public:
    CString() noexcept;
    CString(const char* data, usize length);
    explicit CString(const char* cstr);
    CString(CString&& other) noexcept;
    ~CString();

    CString& operator=(CString&& other) noexcept;

    CString clone() const;
    static CString from_cstr(const char* cstr);

    usize length() const noexcept;
    bool is_empty() const noexcept;
    const char* data() const noexcept;
    const char* c_str() const noexcept;

    char at(usize index) const;
    CStringRef ref() const noexcept;
    CStringRef slice(usize start, usize end) const;
    CString substr(usize start, usize count) const;

    // ---- 前缀/后缀 ----

    bool starts_with(const CStringRef& prefix) const noexcept;
    bool ends_with(const CStringRef& suffix) const noexcept;

    // ---- 修剪 ----

    CStringRef trim() const noexcept;
    CStringRef trim_start() const noexcept;
    CStringRef trim_end() const noexcept;

    // ---- 拆分 ----

    std::vector<CStringRef> split(const CStringRef& delimiter) const;

    // ---- 大小写转换 ----

    CString to_lower() const;
    CString to_upper() const;

    // ---- 替换 ----

    CString replace_all(const CStringRef& from, const CStringRef& to) const;

    // ---- 比较 ----

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


/// @brief 构建 CString 的可变构建器（追加写入，build() 产出）。
class CStringBuilder {
public:
    CStringBuilder() noexcept;
    CStringBuilder(CStringBuilder&& other) noexcept;
    ~CStringBuilder();

    CStringBuilder& operator=(CStringBuilder&& other) noexcept;

    // 拷贝 = delete
    CStringBuilder(const CStringBuilder&) = delete;
    CStringBuilder& operator=(const CStringBuilder&) = delete;

    CStringBuilder& append(const CStringRef& str);
    CStringBuilder& append(const CString& str);
    CStringBuilder& append(const char* cstr);
    CStringBuilder& append(const char* data, usize length);
    CStringBuilder& append(char ch);

    void reserve(usize capacity);
    usize capacity() const noexcept;
    usize length() const noexcept;
    bool is_empty() const noexcept;
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

std::vector<CStringRef> split(const CStringRef& str, const CStringRef& delimiter);
CString join(const std::vector<CStringRef>& parts, const CStringRef& separator);

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
