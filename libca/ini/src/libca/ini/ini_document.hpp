#pragma once

/// @file ini_document.hpp
/// @brief INI 保格式数据模型：IniDocument。
/// @details IniDocument 以"行节点列表"为核心，保留注释、空行、section 顺序和未修改行的
///          原始文本。set/remove 等编辑操作只重建受影响的 key 或 section 行，Writer 按当前
///          行节点顺序输出，从而满足配置文件读改写时尽量不扰动人工注释的需求。
/// @note 采用 Arena 架构：IniDocument 内嵌 `ca::str::Utf8StringArena`，所有字符串字段
///       （IniLine / LineRecord）存 `Utf8StringRef`，指向内部 arena。
///       section 用 Utf8StringRef 表示，空字符串表示全局区（向后兼容约定）。
///       IniDocument 禁拷贝（含 arena，不可共享），仅可移动。document 析构/clear/move-assign
///       后，所有 ref 失效。LineRecord 在 ca::ini::detail 命名空间，属内部实现细节，
///       普通调用方只用 IniLine / get / set / 类型化访问等高层接口。

#include "libca/core/datatype.hpp"
#include "libca/core/result.hpp"

#include "libca/str/utf8_string.hpp"
#include "libca/str/utf8_string_arena.hpp"

#include <map>
#include <string>
#include <vector>

namespace ca::ini {

class IniReader;
class IniWriter;

/// @brief INI 行类型。
enum class IniLineKind
{
    Blank,     ///< 空行。
    Comment,   ///< 注释行。
    Section,   ///< section 声明行。
    KeyValue   ///< key/value 配置项行。
};

/// @brief INI 文档中的一行（只读视图，字符串引用绑定所属 IniDocument）。
struct IniLine
{
    IniLineKind            kind = IniLineKind::Blank;   ///< 行类型。
    ca::str::Utf8StringRef section;   ///< 所属 section；全局区为空字符串。
    ca::str::Utf8StringRef key;       ///< KeyValue 行的 key。
    ca::str::Utf8StringRef value;   ///< KeyValue 行的 value（按解析原样，含可能的首尾引号）。
};

namespace detail {

/// @brief 一行的原始保格式记录（内部实现细节，不保证稳定）。
/// @details Reader 填充，Writer 消费。set() 只重建受影响行的 raw，不动其余行。
struct LineRecord
{
    IniLine                line;          ///< 解析出的高层行信息。
    ca::str::Utf8StringRef raw;           ///< 整行原始文本（不含行结束符）。
    ca::str::Utf8StringRef line_ending;   ///< 行结束符。

    // 以下格式片段仅对 KeyValue 行有效；用于 set() 后原样重建该行。
    ca::str::Utf8StringRef key_prefix;       ///< key 前的缩进/空白。
    ca::str::Utf8StringRef key_suffix;       ///< key 与分隔符之间的空白。
    ca::str::Utf8StringRef separator;        ///< 分隔符（"=" 或 ":"）。
    ca::str::Utf8StringRef value_prefix;     ///< 分隔符与 value 之间的空白。
    ca::str::Utf8StringRef comment_suffix;   ///< value 之后的行内注释（含前导空白）。

    /// value 是否被引号包裹（解析时记录）；set() 重建时按此补回引号。
    bool value_quoted = false;
    /// 若 value_quoted，所用的引号字符（'"' 或 '\''）。
    char value_quote_char = '"';
};

}   // namespace detail

/// @brief INI 文档模型（保格式，Arena 架构）。
class IniDocument
{
public:
    IniDocument();
    IniDocument(const IniDocument&)            = delete;
    IniDocument& operator=(const IniDocument&) = delete;
    IniDocument(IniDocument&&) noexcept;
    IniDocument& operator=(IniDocument&&) noexcept;
    ~IniDocument();

    /// @brief 内部 arena（reader/用户需要 intern 字符串时用）。
    ca::str::Utf8StringArena& arena() noexcept;

    // ---- 查询 ----

    /// @brief 判断 section 是否存在（空字符串表示全局区）。
    bool has_section(const ca::str::Utf8StringRef& section) const;

    /// @brief 判断指定 key 是否存在。
    bool has(const ca::str::Utf8StringRef& section, const ca::str::Utf8StringRef& key) const;

    // ---- 原始值访问 ----

    /// @brief 读取配置值（按解析原样，含可能的首尾引号）。返回 Utf8StringRef，生命周期绑定
    ///        本 document。
    /// @return 成功返回 value；不存在返回错误说明（owning Utf8String）。
    ca::Result<ca::str::Utf8StringRef, ca::str::Utf8String> get(
        const ca::str::Utf8StringRef& section, const ca::str::Utf8StringRef& key) const;

    // ---- 类型化访问（自动剥首尾配对引号后转换） ----

    /// @brief 读取 int 值。自动剥首尾配对引号；解析失败返回错误。
    ca::Result<ca::i64, ca::str::Utf8String> get_int(const ca::str::Utf8StringRef& section,
                                                     const ca::str::Utf8StringRef& key) const;

    /// @brief 读取 double 值。自动剥首尾配对引号；解析失败返回错误。
    ca::Result<ca::f64, ca::str::Utf8String> get_double(const ca::str::Utf8StringRef& section,
                                                        const ca::str::Utf8StringRef& key) const;

    /// @brief 读取 bool 值。接受 true/false/yes/no/on/off/1/0（大小写不敏感）。
    ca::Result<bool, ca::str::Utf8String> get_bool(const ca::str::Utf8StringRef& section,
                                                   const ca::str::Utf8StringRef& key) const;

    /// @brief 读取值；不存在时返回 default_value（返回 Utf8StringRef，生命周期绑定本 document
    ///        或调用方传入的 default_value）。
    ca::str::Utf8StringRef get_or(const ca::str::Utf8StringRef& section,
                                  const ca::str::Utf8StringRef& key,
                                  const ca::str::Utf8StringRef& default_value) const;

    // ---- 编辑 ----

    /// @brief 设置配置值；不存在时会插入到对应 section 末尾。新 value 经 arena.intern 入池。
    /// @note 若原 value 带引号（value_quoted），新 value 重建时补回同样的引号。
    void set(const ca::str::Utf8StringRef& section, const ca::str::Utf8StringRef& key,
             const ca::str::Utf8StringRef& value);

    /// @brief 移除整个 section 以及其下 key/value 行。
    /// @return 实际删除了内容时返回 true。
    bool remove_section(const ca::str::Utf8StringRef& section);

    /// @brief 移除指定 key。
    /// @return key 存在并被删除时返回 true。
    bool remove(const ca::str::Utf8StringRef& section, const ca::str::Utf8StringRef& key);

    // ---- 枚举 ----

    /// @brief 返回 section 列表，按文件中首次出现顺序排列（不含全局区）。
    ///        返回的 Utf8StringRef 生命周期绑定本 document。
    std::vector<ca::str::Utf8StringRef> sections() const;

    /// @brief 返回指定 section 下的 key 列表，按首次出现顺序排列并去重。
    ///        返回的 Utf8StringRef 生命周期绑定本 document。
    std::vector<ca::str::Utf8StringRef> keys(const ca::str::Utf8StringRef& section) const;

    /// @brief 返回解析出的行结构视图（只读）。
    const std::vector<IniLine>& lines() const noexcept;

    /// @brief 清空文档并释放 arena。所有 ref 失效。
    void clear() noexcept;

private:
    friend class IniReader;
    friend class IniWriter;

    // 内部查找：定位到 (section, key) 的 value；找不到返回 nullptr。
    const ca::str::Utf8StringRef* find_value(const ca::str::Utf8StringRef& section,
                                             const ca::str::Utf8StringRef& key) const;

    ca::str::Utf8StringArena        arena_;
    std::vector<detail::LineRecord> records_;
    std::vector<IniLine>            public_lines_;
    // 索引键用 Utf8StringRef（指向 arena 内副本，可拷贝可比较）。
    // 全局区用空 ref 表示。
    std::map<ca::str::Utf8StringRef, ca::usize>                                   section_index_;
    std::map<ca::str::Utf8StringRef, std::map<ca::str::Utf8StringRef, ca::usize>> key_index_;
    std::string default_line_ending_ = "\n";

    // 内部辅助
    void        add_record(detail::LineRecord record);
    void        rebuild_index();
    void        rebuild_key_raw(ca::usize line_index);
    ca::usize   find_insert_position(const ca::str::Utf8StringRef& section) const noexcept;
    std::string line_ending_for_new_line() const;
};

}   // namespace ca::ini
