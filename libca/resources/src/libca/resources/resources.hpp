#pragma once

// libca.resources：嵌入资源库运行时（header-only）。
//
// 组成约定：
// - 资源由 xmake rule(libca.resources.embed) 在构建期把一个或多个挂载目录树
//   生成为 constexpr 头嵌入二进制，单 exe 分发零文件依赖。
// - 目录树以挂载名注册，查询按 bundle 隔离；include 生成头即完成自注册，
//   查询须在动态初始化完成后（main/测试体内）进行。
// - 路径为 Java 式 UTF-8 相对路径：以 '/' 开头、'/' 分隔、大小写敏感；
//   生成期已做 '\' 归一化与字典序排序，查找为二分，过滤迭代为连续指针区间。
// - 纯内存只读视图：不提供任何写盘接口；字节经 ca::core::ByteSlice 暴露。
// - constexpr 方案单文件上限 1 MiB；更大文件待 obj 嵌入方案。
//
// 线程契约：mount/自注册仅发生在静态初始化阶段；此后全部接口只读，可并发调用。

#include <algorithm>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "libca/core/bytes.hpp"
#include "libca/core/datatype.hpp"
#include "libca/core/result.hpp"

namespace ca::resources {

/// 资源访问错误。
enum class ResourceError
{
    NotFound,     ///< bundle 内无此路径
    InvalidPath,  ///< 路径非法：未以 '/' 开头或含 '\'
};

/// 错误转可读字符串，便于日志。
constexpr const char* to_cstr(ResourceError e) noexcept
{
    switch (e) {
    case ResourceError::NotFound: return "resource not found";
    case ResourceError::InvalidPath: return "invalid resource path";
    }
    return "unknown resource error";
}

namespace detail {

/// 路径合法性：非空、以 '/' 开头、不含 '\'（生成期已归一化，查询侧同样拒绝）。
constexpr bool valid_path(std::string_view path) noexcept
{
    if (path.empty() || path.front() != '/') return false;
    if (path.find('\\') != std::string_view::npos) return false;
    return true;
}

}   // namespace detail

/// 生成头条目：路径 + 字节区间（指向 constexpr 存储）。
struct RawEntry
{
    std::string_view path;  ///< 以 '/' 开头的 UTF-8 相对路径（全表字典序有序）
    const ca::u8*    data;  ///< 字节起始；空文件为 nullptr
    ca::usize        size;  ///< 字节数

    /// 文件字节视图（空文件为空视图）。
    ca::core::ByteSlice bytes() const noexcept { return {data, size}; }
};

/// 一棵挂载目录树的只读视图：路径→字节。
///
/// entries 由生成头保证按 path 字典序排列；同目录前缀的条目构成连续区段，
/// 因此 under() 过滤是零分配的指针区间。Bundle 自身可直接 range-for（全量）：
///
///     for (const auto& item : *ca::resources::bundle("crt")) {
///         auto path  = item.path;
///         auto bytes = item.bytes();
///     }
class Bundle
{
public:
    /// 连续条目区间，支持 range-for。
    class Range
    {
    public:
        Range() noexcept = default;
        Range(const RawEntry* first, const RawEntry* last) noexcept : first_(first), last_(last) {}

        const RawEntry* begin() const noexcept { return first_; }
        const RawEntry* end() const noexcept { return last_; }
        ca::usize size() const noexcept { return static_cast<ca::usize>(last_ - first_); }
        bool empty() const noexcept { return first_ == last_; }

    private:
        const RawEntry* first_{nullptr};
        const RawEntry* last_{nullptr};
    };

    constexpr Bundle() noexcept = default;

    /// 由生成头的有序条目数组构造（count 为条目数；空树传 {nullptr, 0}）。
    constexpr Bundle(const RawEntry* entries, ca::usize count) noexcept
        : entries_(entries), count_(count)
    {
    }

    /// 取文件字节；路径非法返回 InvalidPath，未命中返回 NotFound。
    ca::core::Result<ca::core::ByteSlice, ResourceError> get(std::string_view path) const noexcept
    {
        if (!detail::valid_path(path)) return ca::core::Err(ResourceError::InvalidPath);
        const RawEntry* hit = find(path);
        if (hit == nullptr) return ca::core::Err(ResourceError::NotFound);
        return ca::core::Ok(ca::core::ByteSlice{hit->data, hit->size});
    }

    /// 是否存在该路径（非法路径一律 false）。
    bool exists(std::string_view path) const noexcept
    {
        return detail::valid_path(path) && find(path) != nullptr;
    }

    /// 全量条目区间。
    Range all() const noexcept { return Range{entries_, entries_ + count_}; }

    /// 目录前缀过滤（递归）：dir 以 '/' 开头，缺尾 '/' 自动补齐语义
    /// （即 "/a" 等价 "/a/"，且不会误吞 "/ab"）；空串等价全量；无匹配为空区间。
    ///
    /// 实现：把 dir 归一化为必含尾 '/' 的前缀键，二分定位第一个 >= 键的条目，
    /// 再线性推进到首个非前缀条目——同前缀条目在有序表中必然连续。
    /// 每次调用做一次小字符串分配（非热路径，换取平凡的正确性）。
    Range under(std::string_view dir) const
    {
        if (dir.empty()) return all();
        if (!detail::valid_path(dir)) return Range{};
        std::string prefix(dir);
        if (prefix.back() != '/') prefix.push_back('/');
        const RawEntry* end   = entries_ + count_;
        const RawEntry* first = std::lower_bound(
            entries_, end, std::string_view(prefix),
            [](const RawEntry& e, std::string_view key) { return e.path < key; });
        const RawEntry* last = first;
        while (last != end && last->path.size() >= prefix.size() &&
               last->path.compare(0, prefix.size(), prefix) == 0) {
            ++last;
        }
        return Range{first, last};
    }

    /// range-for 支持：全量遍历（等价 all()）。
    const RawEntry* begin() const noexcept { return entries_; }
    /// range-for 支持。
    const RawEntry* end() const noexcept { return entries_ + count_; }

    /// 文件数。
    ca::usize size() const noexcept { return count_; }

private:
    /// 字典序定位精确路径；要求已通过 valid_path。
    const RawEntry* find(std::string_view path) const noexcept
    {
        const RawEntry* end = entries_ + count_;
        const RawEntry* it  = std::lower_bound(
            entries_, end, path,
            [](const RawEntry& e, std::string_view key) { return e.path < key; });
        if (it != end && it->path == path) return it;
        return nullptr;
    }

    const RawEntry* entries_{nullptr};
    ca::usize       count_{0};
};

namespace detail {

/// 挂载索引：name → Bundle。仅在静态初始化阶段写入，之后只读。
inline std::vector<std::pair<std::string_view, const Bundle*>>& bundle_registry()
{
    static std::vector<std::pair<std::string_view, const Bundle*>> instance;
    return instance;
}

}   // namespace detail

/// 注册一个挂载包（生成头经 Registrar 自动调用）。同名重复注册首注册生效，
/// 返回 false；跨 target 撞名时保持首个以保证确定性。
inline bool mount(std::string_view name, const Bundle& bundle)
{
    auto& registry = detail::bundle_registry();
    for (const auto& item : registry) {
        if (item.first == name) return false;
    }
    registry.emplace_back(name, &bundle);
    return true;
}

/// 跨挂载查找；未知名返回 nullptr。
inline const Bundle* bundle(std::string_view name) noexcept
{
    for (const auto& item : detail::bundle_registry()) {
        if (item.first == name) return item.second;
    }
    return nullptr;
}

/// 已注册挂载包数量。
inline ca::usize bundle_count() noexcept
{
    return detail::bundle_registry().size();
}

/// 生成头自注册哨兵：inline 变量保证每个注册点全程序恰好初始化一次。
struct Registrar
{
    Registrar(std::string_view name, const Bundle& bundle) noexcept { mount(name, bundle); }
};

}   // namespace ca::resources
