//
// @brief 不可变 UTF-8 字符串 (Utf8String) 及引用视图 (Utf8StringRef)
// @author Canrad
// @date 2026/05/31
// @note 命名空间 ca::str，基于 u8 类型存储，支持码点访问
//

#ifndef LIBCA_STR_UTF8_STRING_HPP
#define LIBCA_STR_UTF8_STRING_HPP

#include <libca/core/datatype.hpp>

#include <cstddef>
#include <functional>

namespace ca::str {

// ============================================================================
// UTF-8 编解码工具函数
// ============================================================================

// 根据 UTF-8 首字节返回该码点的字节数（1~4），非法首字节返回 0
usize utf8CodePointBytes(u8 firstByte) noexcept;

// 从 UTF-8 字节序列解码出一个码点值
// bytes 必须指向合法的 UTF-8 序列首字节
u32 utf8DecodeCodePoint(const u8* bytes) noexcept;

// 将码点编码为 UTF-8 字节序列写入 out，返回写入的字节数
// 若 cp 超出合法范围 (U+110000 以上) 或为代理项 (U+D800~U+DFFF) 则返回 0
usize utf8EncodeCodePoint(u32 cp, u8* out) noexcept;

// 统计 UTF-8 字节序列中的码点个数
// 若遇到非法序列立即返回 0，并通过 invalidPos 输出非法位置
usize utf8CountCodePoints(const u8* data, usize byteLength,
                          usize* invalidPos = nullptr) noexcept;

// 检查字节序列是否为合法 UTF-8
bool utf8IsValid(const u8* data, usize byteLength) noexcept;


// ============================================================================
// 前向声明
// ============================================================================

class Utf8String;


// ============================================================================
// Utf8StringRef — 非拥有引用的不可变 UTF-8 字符串视图
// ============================================================================

class Utf8StringRef {
public:
    // 默认构造：空视图
    Utf8StringRef() noexcept;

    // 从字节数据 + 码点个数构造（不校验，不复制）
    // data: UTF-8 字节数据指针
    // byteLength: 字节长度
    // length: 码点个数
    Utf8StringRef(const u8* data, usize byteLength, usize length) noexcept;

    // 从 Utf8String 构造视图
    Utf8StringRef(const Utf8String& str) noexcept;

    // ---- 查询 ----

    // 码点个数（O(1)）
    usize length() const noexcept;

    // 字节长度（O(1)）
    usize byteLength() const noexcept;

    // 是否为空字符串
    bool isEmpty() const noexcept;

    // 原始字节数据指针（非空终止）
    const u8* data() const noexcept;

    // ---- 访问 ----

    // 按字节下标访问（O(1)），不进行边界检查
    u8 byteAt(usize index) const;

    // 按码点下标访问（O(n)），不进行边界检查
    u32 codePointAt(usize index) const;

    // ---- 切片（返回 Utf8StringRef，不分配内存） ----

    // 按字节区间 [byteStart, byteEnd) 切片
    Utf8StringRef slice(usize byteStart, usize byteEnd) const;

    // 按码点区间切片：从第 cpStart 个码点开始，取 cpCount 个码点
    Utf8StringRef sliceByCp(usize cpStart, usize cpCount) const;

    // ---- 子串（返回 Utf8String，分配内存） ----

    // 按码点取子串：从第 cpStart 个码点开始，取 cpCount 个码点
    Utf8String substr(usize cpStart, usize cpCount) const;

    // ---- 比较 ----

    // 逐字节字典序比较
    int compare(const Utf8StringRef& other) const noexcept;

    // 内容相等判断（逐字节比较）
    bool equals(const Utf8StringRef& other) const noexcept;

    bool operator==(const Utf8StringRef& other) const noexcept;
    bool operator!=(const Utf8StringRef& other) const noexcept;

private:
    const u8* data_;
    usize     byteLength_;
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
    // byteLength: 字节长度
    Utf8String(const u8* data, usize byteLength);

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

    // 从单个码点创建 UTF-8 字符串
    static Utf8String fromCodePoint(u32 cp);

    // 从字节数组创建（同构造函数语义）
    static Utf8String fromUtf8(const u8* data, usize byteLength);

    // ---- 查询 ----

    // 码点个数（O(1)）
    usize length() const noexcept;

    // 字节长度（O(1)）
    usize byteLength() const noexcept;

    // 是否为空字符串
    bool isEmpty() const noexcept;

    // 原始字节数据指针（内部存储以 '\0' 结尾）
    const u8* data() const noexcept;

    // C 风格字符串（O(1)，内部已有 '\0' 终止符）
    const char* cStr() const noexcept;

    // ---- 访问 ----

    // 按字节下标访问（O(1)），不进行边界检查
    u8 byteAt(usize index) const;

    // 按码点下标访问（O(n) 扫描），不进行边界检查
    u32 codePointAt(usize index) const;

    // ---- 视图 / 切片 ----

    // 获取整个字符串的 Utf8StringRef 视图
    Utf8StringRef ref() const noexcept;

    // 按字节区间 [byteStart, byteEnd) 切片，返回非拥有视图
    Utf8StringRef slice(usize byteStart, usize byteEnd) const;

    // 按码点区间切片：从第 cpStart 个码点开始，取 cpCount 个码点
    Utf8StringRef sliceByCp(usize cpStart, usize cpCount) const;

    // ---- 子串 ----

    // 按码点取子串：从第 cpStart 个码点开始，取 cpCount 个码点
    Utf8String substr(usize cpStart, usize cpCount) const;

    // ---- 比较 ----

    int compare(const Utf8StringRef& other) const noexcept;
    int compare(const Utf8String& other) const noexcept;

    bool equals(const Utf8StringRef& other) const noexcept;

    bool operator==(const Utf8String& other) const noexcept;
    bool operator==(const Utf8StringRef& other) const noexcept;
    bool operator!=(const Utf8String& other) const noexcept;
    bool operator!=(const Utf8StringRef& other) const noexcept;

private:
    u8*   data_;
    usize byteLength_;
    usize length_;   // 码点个数（缓存）

    // 内部初始化：从 src 复制 byteLen 字节，校验 UTF-8，计算码点个数
    void init(const u8* src, usize byteLen);
};


// ============================================================================
// 非成员比较运算符（对称比较）
// ============================================================================

bool operator==(const Utf8StringRef& lhs, const Utf8String& rhs) noexcept;
bool operator!=(const Utf8StringRef& lhs, const Utf8String& rhs) noexcept;

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

#endif  // LIBCA_STR_UTF8_STRING_HPP
