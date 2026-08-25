//
// @brief 不可变宽字符串 (WString)、引用视图 (WStringRef)
//        及可变构建器 (WStringBuilder)
// @author Canrad
// @date 2026/05/31
// @note 命名空间 ca::str，基于 wchar_t 类型存储
//

#pragma once

#include "libca/core/datatype.hpp"

#include <cstddef>
#include <functional>
#include <string>
#include <vector>

namespace ca::str {

class WString;

/// @brief 非拥有、不可变的宽字符串（wchar_t）视图。
/// @warning 不拥有数据；来自 WString::ref()/slice() 的视图在原串销毁或移动后失效。
/// @note 不保证结尾 `\0`（中段切片不能假设以 `\0` 结尾），故不提供 w_str()。
class WStringRef {
public:
    /// 空视图（data() 为 nullptr）。
    WStringRef() noexcept;

    /// @brief 从指针 + 长度构造视图（零拷贝，不校验内容）。
    /// @param data 宽字符数据指针  @param length 宽字符个数
    WStringRef(const wchar_t* data, usize length) noexcept;

    /// 从 WString 构造视图（引用其内部数据）。
    WStringRef(const WString& str) noexcept;

    /// 宽字符个数（O(1)）。
    usize length() const noexcept;

    /// 是否为空视图。
    bool is_empty() const noexcept;

    /// 原始数据指针（不保证 `\0` 终止；空视图为 nullptr）。
    const wchar_t* data() const noexcept;

    /// 按下标访问（O(1)），不进行边界检查。
    wchar_t at(usize index) const;

    /// @brief 取 [start, end) 区间的非拥有视图。
    /// @note 区间为空或 start 越界返回空视图；end 超出长度时截断到末尾。
    WStringRef slice(usize start, usize end) const;

    /// @brief 从 start 开始取 count 个字符，深拷贝为拥有型子串。
    /// @note 越界语义同 slice()，越界部分被截断。
    WString substr(usize start, usize count) const;

    // ---- 前缀/后缀 ----
    /// 是否以 prefix 开头。
    bool starts_with(const WStringRef& prefix) const noexcept;

    /// 是否以 suffix 结尾。
    bool ends_with(const WStringRef& suffix) const noexcept;

    // ---- 修剪 ----
    /// 去除两端的空白字符（iswspace 判定），返回视图，不分配。
    WStringRef trim() const noexcept;

    /// 去除开头的空白字符（iswspace 判定），返回视图，不分配。
    WStringRef trim_start() const noexcept;

    /// 去除结尾的空白字符（iswspace 判定），返回视图，不分配。
    WStringRef trim_end() const noexcept;

    // ---- 拆分 ----
    /// @brief 按分隔符拆分为非拥有视图列表。
    /// @note 空串返回空列表；分隔符为空返回仅含自身的一个元素；
    ///       连续或结尾的分隔符会产生空片段。
    std::vector<WStringRef> split(const WStringRef& delimiter) const;

    // ---- 大小写转换 ----
    /// @brief 转为小写，返回新串。
    /// @note 逐 wchar_t 调用 towlower，受当前 C locale 影响；
    ///       Windows 上不感知 surrogate pair。
    WString to_lower() const;

    /// @brief 转为大写，返回新串。
    /// @note 逐 wchar_t 调用 towupper，受当前 C locale 影响；
    ///       Windows 上不感知 surrogate pair。
    WString to_upper() const;

    // ---- 替换 ----
    /// @brief 将子串 from 的全部出现（从左到右、不重叠）替换为 to，返回新串。
    /// @note from 为空时返回原内容的拷贝；to 可为空（即删除）。
    WString replace_all(const WStringRef& from, const WStringRef& to) const;

    // ---- 比较 ----

    /// 按 wchar_t 逐字符字典序比较，返回负值 / 0 / 正值。
    int compare(const WStringRef& other) const noexcept;

    /// 内容相等判断（长度相同且逐字符相等）。
    bool equals(const WStringRef& other) const noexcept;

    /// 内容相等判断（同 equals()）。
    bool operator==(const WStringRef& other) const noexcept;

    /// 内容不等判断（equals() 取反）。
    bool operator!=(const WStringRef& other) const noexcept;

private:
    const wchar_t* data_;
    usize          length_;
};

/// @brief 拥有所有权、不可变、内部以 `\0` 终止的宽字符串。
/// @note 禁止隐式拷贝（须显式 clone()），可移动。保证结尾 `\0`，故提供 w_str()。
class WString {
public:
    /// 默认构造：空字符串（data() 指向合法的 `\0` 终止空串）。
    WString() noexcept;

    /// @brief 从指针 + 长度深拷贝构造，允许内嵌 `\0`。
    /// @note data 为空指针或 length 为 0 时构造空字符串。
    WString(const wchar_t* data, usize length);

    /// 从以 `\0` 结尾的 C 宽字符串构造（O(n) 调用 wcslen）；空指针构造空字符串。
    explicit WString(const wchar_t* wstr);

    /// @brief 移动构造。
    /// @note 被移动方置为空，其 data()/w_str() 为 nullptr（区别于默认构造的空串），
    ///       仅可重新赋值或析构。
    WString(WString&& other) noexcept;

    /// 释放内部缓冲区。
    ~WString();

    /// @brief 移动赋值（自赋值安全）。
    /// @note 被移动方 data()/w_str() 变为 nullptr。
    WString& operator=(WString&& other) noexcept;

    /// 显式深拷贝（唯一的复制方式）。
    WString clone() const;

    /// 从以 `\0` 结尾的 C 宽字符串构造；空指针返回空字符串。
    static WString from_wstr(const wchar_t* wstr);

    /// 宽字符个数（O(1)，不含结尾 `\0`）。
    usize length() const noexcept;

    /// 是否为空字符串。
    bool is_empty() const noexcept;

    /// 原始数据指针（保证 `\0` 终止，终止符不计入 length()）。
    const wchar_t* data() const noexcept;

    /// C 风格宽字符串（与 data() 相同）。
    /// @note 内容含内嵌 `\0` 时，按 C 语义会在该处截断。
    const wchar_t* w_str() const noexcept;

    /// 按下标访问（O(1)），不进行边界检查。
    wchar_t at(usize index) const;

    /// 获取整个字符串的非拥有视图。
    WStringRef ref() const noexcept;

    /// 取 [start, end) 区间的非拥有视图（越界语义同 WStringRef::slice()）。
    WStringRef slice(usize start, usize end) const;

    /// 从 start 开始取 count 个字符，深拷贝为拥有型子串（越界部分截断）。
    WString substr(usize start, usize count) const;

    // ---- 前缀/后缀 ----

    /// 是否以 prefix 开头。
    bool starts_with(const WStringRef& prefix) const noexcept;

    /// 是否以 suffix 结尾。
    bool ends_with(const WStringRef& suffix) const noexcept;

    // ---- 修剪 ----

    /// 去除两端的空白字符（iswspace 判定），返回引用自身数据的视图，不分配。
    WStringRef trim() const noexcept;

    /// 去除开头的空白字符（iswspace 判定），返回视图，不分配。
    WStringRef trim_start() const noexcept;

    /// 去除结尾的空白字符（iswspace 判定），返回视图，不分配。
    WStringRef trim_end() const noexcept;

    // ---- 拆分 ----

    /// 按分隔符拆分为非拥有视图列表（语义同 WStringRef::split()）。
    std::vector<WStringRef> split(const WStringRef& delimiter) const;

    // ---- 大小写转换 ----

    /// 转为小写，返回新串（语义同 WStringRef::to_lower()）。
    WString to_lower() const;

    /// 转为大写，返回新串（语义同 WStringRef::to_upper()）。
    WString to_upper() const;

    // ---- 替换 ----

    /// 将子串 from 的全部出现替换为 to，返回新串（语义同 WStringRef::replace_all()）。
    WString replace_all(const WStringRef& from, const WStringRef& to) const;

    // ---- 比较 ----

    /// 按 wchar_t 逐字符字典序比较视图，返回负值 / 0 / 正值。
    int compare(const WStringRef& other) const noexcept;

    /// 与另一 WString 按 wchar_t 逐字符字典序比较，返回负值 / 0 / 正值。
    int compare(const WString& other) const noexcept;

    /// 内容相等判断。
    bool equals(const WStringRef& other) const noexcept;

    /// 内容相等判断（同 equals()）。
    bool operator==(const WString& other) const noexcept;

    /// 内容相等判断（同 equals()）。
    bool operator==(const WStringRef& other) const noexcept;

    /// 内容不等判断。
    bool operator!=(const WString& other) const noexcept;

    /// 内容不等判断。
    bool operator!=(const WStringRef& other) const noexcept;

private:
    wchar_t* data_;
    usize    length_;
    void init(const wchar_t* src, usize len);
};

/// @brief 构建 WString 的可变构建器（追加写入，build() 产出）。
/// @note move-only，禁止拷贝。内部缓冲区按需倍增扩容，容量为 wchar_t 个数。
class WStringBuilder {
public:
    /// 默认构造：空构建器（预分配默认容量）。
    WStringBuilder() noexcept;

    /// @brief 移动构造。
    /// @note 被移动方置为空，仅可重新赋值或析构。
    WStringBuilder(WStringBuilder&& other) noexcept;

    /// 释放内部缓冲区。
    ~WStringBuilder();

    /// 移动赋值（自赋值安全；被移动方置空）。
    WStringBuilder& operator=(WStringBuilder&& other) noexcept;

    // 拷贝 = delete
    WStringBuilder(const WStringBuilder&) = delete;
    WStringBuilder& operator=(const WStringBuilder&) = delete;

    /// 追加视图内容；返回 *this 支持链式调用。
    WStringBuilder& append(const WStringRef& str);

    /// 追加字符串内容。
    WStringBuilder& append(const WString& str);

    /// 追加以 `\0` 结尾的 C 宽字符串（O(n) 调用 wcslen）；空指针无操作。
    WStringBuilder& append(const wchar_t* wstr);

    /// @brief 追加指定长度的宽字符序列（不要求 `\0` 终止）。
    /// @param data 数据指针  @param length 宽字符个数
    WStringBuilder& append(const wchar_t* data, usize length);

    /// 追加单个字符。
    WStringBuilder& append(wchar_t ch);

    /// 预留容量（仅扩容，不缩容）。
    void reserve(usize capacity);

    /// 当前缓冲区容量（wchar_t 个数）。
    usize capacity() const noexcept;

    /// 已追加的宽字符个数。
    usize length() const noexcept;

    /// 是否尚未追加任何内容。
    bool is_empty() const noexcept;

    /// 清空已追加内容（重置长度，保留缓冲区）。
    void clear() noexcept;

    /// @brief 深拷贝当前内容生成 WString。
    /// @note 构建器不被消耗，可继续追加后再次 build()。
    WString build() const;

private:
    wchar_t* buffer_;
    usize    length_;
    usize    capacity_;
    static constexpr usize kDefaultCapacity = 64;
    void grow(usize minCapacity);
};

/// 提供 `WStringRef == WString` 的对称相等比较。
bool operator==(const WStringRef& lhs, const WString& rhs) noexcept;

/// 提供 `WStringRef != WString` 的对称不等比较。
bool operator!=(const WStringRef& lhs, const WString& rhs) noexcept;

/// 按分隔符拆分视图（等价于 str.split(delimiter)）。
std::vector<WStringRef> split(const WStringRef& str, const WStringRef& delimiter);

/// 用分隔符连接多个视图，返回新串（空列表返回空串）。
WString join(const std::vector<WStringRef>& parts, const WStringRef& separator);

}  // namespace ca::str

namespace std {

/// 支持 WString 作为 unordered 容器 key 的 std::hash 特化。
template <>
struct hash<ca::str::WString> {
    /// 按内容计算 FNV-1a 64 位哈希（逐 wchar_t）。
    size_t operator()(const ca::str::WString& s) const noexcept;
};

/// 支持 WStringRef 作为 unordered 容器 key 的 std::hash 特化。
template <>
struct hash<ca::str::WStringRef> {
    /// 按内容计算 FNV-1a 64 位哈希（逐 wchar_t）。
    size_t operator()(const ca::str::WStringRef& s) const noexcept;
};

}  // namespace std
