//
// @brief 不可变 UTF-8 字符串 (Utf8String) 及引用视图 (Utf8StringRef)
// @author Canrad
// @date 2026/05/31
// @note 命名空间 ca::str，基于 u8 类型存储，支持码点访问
//

#pragma once

#include "libca/core/datatype.hpp"

#include "utf8_util.hpp"

#include <cstddef>
#include <functional>
#include <ostream>
#include <string>
#include <vector>

namespace ca::str {

// ============================================================================
// 前向声明
// ============================================================================

class Utf8String;
class Utf8Iterator;
class ZUtf8StringRef;


// ============================================================================
// Utf8StringRef — 非拥有引用的不可变 UTF-8 字符串视图
// ============================================================================

class Utf8StringRef {
public:
    // 默认构造：空视图
    Utf8StringRef() noexcept;

    // 从字节数据 + 码点个数构造（不校验，不复制）
    // data: UTF-8 字节数据指针
    // byte_length: 字节长度
    // length: 码点个数
    Utf8StringRef(const u8* data, usize byte_length, usize length) noexcept;

    // 从 Utf8String 构造视图
    Utf8StringRef(const Utf8String& str) noexcept;

    // 下面这两个构建都是O(n)，因为要计算码点个数
    // 从 C 字符串构造视图
    static Utf8StringRef from_cstr(const char* cstr) noexcept;
    // 从二进制数据创建视图，如果cp_len不传入，那么会重新计算码点
    static Utf8StringRef from_data(const u8* data, usize byte_len, usize cp_len = -1);

    // 特殊常量
    static constexpr usize npos = usize(-1);

    // ---- 查询 ----

    // 码点个数（O(1)）
    usize length() const noexcept;

    // 字节长度（O(1)）
    usize byte_length() const noexcept;

    // 是否为空字符串
    bool is_empty() const noexcept;

    // 原始字节数据指针（非空终止）
    const u8* data() const noexcept;

    // 不提供c_str的根本原因是 拿到原始的c风格字符串，如果UTF8字符串内部有\0，那么不能保证可用性，
    // 其次如果只为了方便提供一个const char*版本的原始字符串，而且因为是ref，所以如果是别人字符串的中间切片的ref，也不能随意修改加\0
    // 所以c_str这种导出一个const char*的接口不能在0拷贝情况下存在
    // const char* c_str();

    // ---- 访问 ----

    // 按字节下标访问（O(1)），不进行边界检查
    u8 byte_at(usize index) const;

    // 按码点下标访问（O(n)），不进行边界检查
    u32 code_point_at(usize index) const;

    // ---- 切片（返回 Utf8StringRef，不分配内存） ----

    // 按字节区间 [byte_start, byte_end) 切片
    Utf8StringRef slice(usize byte_start, usize byte_end) const;

    // 按码点区间切片：从第 cp_start 个码点开始，取 cp_count 个码点
    Utf8StringRef slice_by_cp(usize cp_start, usize cp_count) const;

    // ---- 子串（返回 Utf8String，分配内存） ----

    // 按码点取子串：从第 cp_start 个码点开始，取 cp_count 个码点
    Utf8String substr(usize cp_start, usize cp_count) const;

    // ---- 前缀/后缀检查（字节级，O(n)）----

    bool starts_with(const Utf8StringRef& prefix) const noexcept;
    bool ends_with(const Utf8StringRef& suffix) const noexcept;

    // ---- 修剪（返回视图，不分配） ----

    Utf8StringRef trim() const noexcept;
    Utf8StringRef trim_start() const noexcept;
    Utf8StringRef trim_end() const noexcept;

    // ---- 拆分 ----

    std::vector<Utf8StringRef> split(const Utf8StringRef& delimiter) const;

    // ---- 大小写转换（返回 Utf8String，分配） ----

    Utf8String to_lower() const;
    Utf8String to_upper() const;

    // ---- 替换 ----

    Utf8String replace_all(const Utf8StringRef& from, const Utf8StringRef& to) const;

    // ---- 查找 ----

    // 查找子串首次出现的码点下标，未找到返回 npos
    usize index_of(const Utf8StringRef& needle) const noexcept;
    usize index_of(const Utf8StringRef& needle, usize start_cp) const noexcept;
    usize index_of(u32 code_point) const noexcept;
    usize index_of(u32 code_point, usize start_cp) const noexcept;

    // 是否包含子串
    bool contains(const Utf8StringRef& needle) const noexcept;

    // ---- 迭代器 ----

    Utf8Iterator begin() const noexcept;
    Utf8Iterator end() const noexcept;

    // ---- 比较 ----

    // 逐字节字典序比较
    int compare(const Utf8StringRef& other) const noexcept;
    int compare(const char* cstr) const noexcept;

    // 内容相等判断（逐字节比较）
    bool equals(const Utf8StringRef& other) const noexcept;
    bool equals(const char* cstr) const noexcept;

    bool operator==(const Utf8StringRef& other) const noexcept;
    bool operator==(const char* cstr) const noexcept;
    bool operator!=(const Utf8StringRef& other) const noexcept;
    bool operator!=(const char* cstr) const noexcept;

    // 创建标准库的字符串
    std::string to_std_string() const;
private:
    const u8* data_;
    usize     byte_length_;
    usize     length_;   // 码点个数
};


// ============================================================================
// Utf8String — 拥有所有权的不可变 UTF-8 字符串
// ============================================================================

class Utf8String {
public:
    // ---- 构造 / 析构 ----

    // 默认构造：空字符串
    Utf8String() noexcept;

    // 从字节数组构造（复制 + 校验 UTF-8 合法性）
    // data: UTF-8 字节数据（不必以 '\0' 结尾）
    // byte_length: 字节长度
    Utf8String(const u8* data, usize byte_length);

    // 从 C 风格字符串构造（null-terminated）
    explicit Utf8String(const char* cstr);

    // 拷贝构造（已删除，请使用 clone()）
    Utf8String(const Utf8String& other) = delete;

    // 移动构造
    Utf8String(Utf8String&& other) noexcept;

    // 析构
    ~Utf8String();

    // 拷贝赋值（已删除，请使用 clone()）
    Utf8String& operator=(const Utf8String& other) = delete;

    // 移动赋值
    Utf8String& operator=(Utf8String&& other) noexcept;

    // 显式克隆（唯一复制方式）
    Utf8String clone() const;

    // ---- 工厂方法 ----

    // 从 C 字符串构造
    static Utf8String from_cstr(const char* cstr) noexcept;
    // 从utf8字符串数据构造
    static Utf8String from_data(const u8* data, usize byte_len, usize cp_len = -1);
    // 从单个码点创建 UTF-8 字符串
    static Utf8String from_code_point(u32 cp);

    // ---- 查询 ----

    // 码点个数（O(1)）
    usize length() const noexcept;

    // 字节长度（O(1)）
    usize byte_length() const noexcept;

    // 是否为空字符串
    bool is_empty() const noexcept;

    // STL 兼容别名
    usize size() const noexcept { return length_; }
    bool empty() const noexcept { return byte_length_ == 0; }

    // 原始字节数据指针（内部存储以 '\0' 结尾）
    const u8* data() const noexcept;

    // C 风格字符串（O(1)，内部已有 '\0' 终止符）
    const char* c_str() const noexcept;

    // ---- 访问 ----

    // 按字节下标访问（O(1)），不进行边界检查
    u8 byte_at(usize index) const;

    // 按码点下标访问（O(n) 扫描），不进行边界检查
    u32 code_point_at(usize index) const;

    // ---- 视图 / 切片 ----

    // 获取整个字符串的 Utf8StringRef 视图
    Utf8StringRef ref() const noexcept;

    // 按字节区间 [byte_start, byte_end) 切片，返回非拥有视图
    Utf8StringRef slice(usize byte_start, usize byte_end) const;

    // 按码点区间切片：从第 cp_start 个码点开始，取 cp_count 个码点
    Utf8StringRef slice_by_cp(usize cp_start, usize cp_count) const;

    // ---- 迭代器 ----

    Utf8Iterator begin() const noexcept;
    Utf8Iterator end() const noexcept;

    // ---- 子串 ----

    // 按码点取子串：从第 cp_start 个码点开始，取 cp_count 个码点
    Utf8String substr(usize cp_start, usize cp_count) const;

    // ---- 前缀/后缀 ----

    bool starts_with(const Utf8StringRef& prefix) const noexcept;
    bool ends_with(const Utf8StringRef& suffix) const noexcept;

    // ---- 修剪 ----

    Utf8StringRef trim() const noexcept;
    Utf8StringRef trim_start() const noexcept;
    Utf8StringRef trim_end() const noexcept;

    // ---- 拆分 ----

    std::vector<Utf8StringRef> split(const Utf8StringRef& delimiter) const;

    // ---- 大小写转换 ----

    Utf8String to_lower() const;
    Utf8String to_upper() const;

    // ---- 替换 ----

    Utf8String replace_all(const Utf8StringRef& from, const Utf8StringRef& to) const;

    // ---- 查找 ----

    usize index_of(const Utf8StringRef& needle) const noexcept;
    usize index_of(const Utf8StringRef& needle, usize start_cp) const noexcept;
    usize index_of(u32 code_point) const noexcept;
    usize index_of(u32 code_point, usize start_cp) const noexcept;
    bool contains(const Utf8StringRef& needle) const noexcept;

    // ---- 比较 ----

    int compare(const Utf8StringRef& other) const noexcept;
    int compare(const Utf8String& other) const noexcept;
    int compare(const char* cstr) const noexcept;
    bool equals(const Utf8StringRef& other) const noexcept;
    bool equals(const char* cstr) const noexcept;
    bool operator==(const Utf8String& other) const noexcept;
    bool operator==(const Utf8StringRef& other) const noexcept;
    bool operator==(const char* cstr) const noexcept;
    bool operator!=(const Utf8String& other) const noexcept;
    bool operator!=(const Utf8StringRef& other) const noexcept;
    bool operator!=(const char* cstr) const noexcept;

    // 创建标准库的字符串
    std::string to_std_string() const;

private:
    u8*   data_;
    usize byte_length_;
    usize length_;   // 码点个数（缓存）

    // 内部初始化：从 src 复制 byte_len 字节，校验 UTF-8，计算码点个数
    void init(const u8* src, usize byte_len);
};


// ============================================================================
// Utf8StringBuilder — 用于构建 Utf8String 的可变构建器
// ============================================================================

class Utf8StringBuilder {
public:
    Utf8StringBuilder() noexcept;
    Utf8StringBuilder(Utf8StringBuilder&& other) noexcept;
    ~Utf8StringBuilder();

    Utf8StringBuilder& operator=(Utf8StringBuilder&& other) noexcept;

    Utf8StringBuilder(const Utf8StringBuilder&) = delete;
    Utf8StringBuilder& operator=(const Utf8StringBuilder&) = delete;

    Utf8StringBuilder& append(const Utf8StringRef& str);
    Utf8StringBuilder& append(const Utf8String& str);
    Utf8StringBuilder& append(const char* cstr);
    Utf8StringBuilder& append(const u8* data, usize byte_length);
    bool append_code_point(u32 cp);

    void reserve(usize byte_capacity);
    usize capacity() const noexcept;
    usize byte_length() const noexcept;
    bool is_empty() const noexcept;
    void clear() noexcept;

    Utf8String build() const;
    Utf8String build_or_empty() const noexcept;

private:
    u8*   buffer_;
    usize byte_length_;
    usize capacity_;
    static constexpr usize DEFAULT_CAPACITY = 64;
    void grow(usize min_capacity);
};


// ============================================================================
// Utf8Iterator — 前向迭代器，O(1) 步进，配合范围 for 使用
// ============================================================================

class Utf8Iterator {
public:
    using iterator_category = std::forward_iterator_tag;
    using value_type        = u32;
    using difference_type   = std::ptrdiff_t;
    using pointer           = const u32*;
    using reference         = u32;

    Utf8Iterator() noexcept : pos_(nullptr), end_(nullptr) {}
    Utf8Iterator(const u8* pos, const u8* end) noexcept : pos_(pos), end_(end) {}

    u32 operator*() const noexcept {
        return utf8_decode_code_point(pos_);
    }

    Utf8Iterator& operator++() noexcept {
        pos_ += utf8_code_point_bytes_safe(*pos_);
        return *this;
    }

    Utf8Iterator operator++(int) noexcept {
        Utf8Iterator tmp = *this;
        pos_ += utf8_code_point_bytes_safe(*pos_);
        return tmp;
    }

    bool operator==(const Utf8Iterator& other) const noexcept {
        return pos_ == other.pos_;
    }

    bool operator!=(const Utf8Iterator& other) const noexcept {
        return pos_ != other.pos_;
    }

    const u8* byte_ptr() const noexcept { return pos_; }

private:
    const u8* pos_;
    const u8* end_;
};


// ============================================================================
// 非成员比较运算符（对称比较）
// ============================================================================

bool operator==(const Utf8StringRef& lhs, const Utf8String& rhs) noexcept;
bool operator!=(const Utf8StringRef& lhs, const Utf8String& rhs) noexcept;
bool operator==(const char* lhs, const Utf8StringRef& rhs) noexcept;
bool operator!=(const char* lhs, const Utf8StringRef& rhs) noexcept;
bool operator==(const char* lhs, const Utf8String& rhs) noexcept;
bool operator!=(const char* lhs, const Utf8String& rhs) noexcept;
bool operator==(const char* lhs, const ZUtf8StringRef& rhs) noexcept;
bool operator!=(const char* lhs, const ZUtf8StringRef& rhs) noexcept;

/// 按分隔符拆分为视图列表
std::vector<Utf8StringRef> split(const Utf8StringRef& str,
                                 const Utf8StringRef& delimiter);

/// 用分隔符连接多个字符串
Utf8String join(const std::vector<Utf8StringRef>& parts,
                const Utf8StringRef& separator);

/// 流输出
std::ostream& operator<<(std::ostream& os, const Utf8StringRef& s);
std::ostream& operator<<(std::ostream& os, const Utf8String& s);

// 针对\0结尾的字面量，也就是"test"这样子的字面量，解决既不能存Utf8String也不能存Utf8StringRef的问题
// 这个问题的根本出发点是有两个原因，一个是Utf8String这玩意需要分配内存，实际上一个字面量不需要的
// 它仅仅需要一个const char*加上一个usize就行，而且不需要任何拷贝。
// 不用Utf8StringRef的原因是，没有所有权，使用没办法保证\0，虽然from_cstr看上去可以做缓存
// 但是实际上，动态内存也会因此加入，没法都缓存
// 所以我搞一个ZUtf8String从语义上保证这个问题，ZUtf8String是零结尾字符串视图
// 一定是\0结尾，而且是字面量，可以做缓存的语义
// 对于ZUtf8StringRef一定要做成全局变量或者是static，避免重复计算码点
class ZUtf8StringRef{
public:
    // 从 C 字符串构造（仅建议用于字面量或全局常量），
    // 只有这个from_static会吃到全局缓存表的查询优化，其他没有优化，
    // 其次这个不保证去重的，如果要去重那么需要基于编译器的-fmerge-constants保证每个字符串常量一定是在一个地址上
    static ZUtf8StringRef from_static(const char* cstr);

    // 从 Utf8String 转换（Utf8String 保证末尾有 \0）
    static ZUtf8StringRef from_utf8_string(const Utf8String& s);

    // 从 std::string转换，自行保证std::string的内容
    static ZUtf8StringRef from_std_string(const std::string& s);

    // 不提供从普通 Utf8StringRef 的隐式转换，防止传入不保证 \0 的视图

    const u8* data() const { return data_; }
    usize byte_length() const { return byte_length_; }
    usize length() const { return cp_length_; }
    const char* c_str() const { return reinterpret_cast<const char*>(data_); } // 安全，保证 \0
    Utf8StringRef ref() const noexcept;
    operator Utf8StringRef() const noexcept;

    int compare(const Utf8StringRef& other) const noexcept;
    int compare(const char* cstr) const noexcept;
    bool equals(const Utf8StringRef& other) const noexcept;
    bool equals(const char* cstr) const noexcept;
    bool operator==(const Utf8StringRef& other) const noexcept;
    bool operator==(const char* cstr) const noexcept;
    bool operator!=(const Utf8StringRef& other) const noexcept;
    bool operator!=(const char* cstr) const noexcept;

private:
    const u8* data_;
    usize byte_length_;
    usize cp_length_;

    ZUtf8StringRef(const u8* d, usize bl, usize cl) : data_(d), byte_length_(bl), cp_length_(cl) {}
};


}  // namespace ca::str


// ============================================================================
// std::hash 特化
// ============================================================================

namespace std {

template <>
struct hash<ca::str::Utf8String> {
    size_t operator()(const ca::str::Utf8String& s) const noexcept;
};

template <>
struct hash<ca::str::Utf8StringRef> {
    size_t operator()(const ca::str::Utf8StringRef& s) const noexcept;
};

}  // namespace std
