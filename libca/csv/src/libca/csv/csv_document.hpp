#pragma once

/// @file csv_document.hpp
/// @brief CSV 数据模型，提供 CsvRow 与 CsvDocument 两层结构。
/// @details 模型只表达表格数据本身，不保存 CSV 原始空白或换行风格；这些格式策略由
///          CsvReaderOptions 与 CsvWriterOptions 控制。字段内容按解析后的真实值保存，
///          写回时由 CsvWriter 负责按需加引号与转义。
/// @note 采用 Arena 架构：`CsvDocument` 内嵌 `Utf8StringArena`，所有字段经
///       `arena.intern_raw(...)` 入池（不校验 UTF-8，按原始字节入池——CSV 不规定编码，
///       字段可能含任意字节）。`CsvRow` 与 `CsvDocument` 的字段存 `Utf8StringRef`，
///       生命周期绑定 CsvDocument；document 析构/clear/move-assign 后所有 ref 失效。
///       CsvDocument 禁拷贝（含 arena，不可共享），仅可移动。

#include "libca/core/datatype.hpp"

#include "libca/str/utf8_string.hpp"
#include "libca/str/utf8_string_arena.hpp"

#include <vector>

namespace ca::csv {

class CsvReader;
class CsvWriter;

/// @brief CSV 的一行记录。
/// @details CsvRow 是字段 Utf8StringRef 的轻量包装，保留字段顺序，允许字段为空字符串。
///          字段 ref 生命周期绑定所属 CsvDocument。
class CsvRow
{
public:
    /// @brief 构造空行。
    CsvRow() = default;

    /// @brief 从已 intern 的字段列表构造一行。
    /// @param fields 按 CSV 列顺序排列的字段（Utf8StringRef，调用方负责经
    ///               CsvDocument::intern_field 入池）。
    explicit CsvRow(std::vector<ca::str::Utf8StringRef> fields);

    /// @brief 返回字段数量。
    /// @return 当前列数。
    ca::usize size() const noexcept;

    /// @brief 判断是否没有字段。
    /// @return 没有字段时返回 true。
    bool empty() const noexcept;

    /// @brief 访问指定字段。
    /// @param index 字段下标。
    /// @return 字段引用。
    /// @warning 不做边界检查，调用方需保证 index 合法。
    const ca::str::Utf8StringRef& operator[](ca::usize index) const noexcept;

    /// @brief 访问指定字段。
    /// @param index 字段下标。
    /// @return 可修改字段引用。
    /// @warning 不做边界检查，调用方需保证 index 合法。
    ca::str::Utf8StringRef& operator[](ca::usize index) noexcept;

    /// @brief 获取所有字段。
    /// @return 字段列表的只读引用。
    const std::vector<ca::str::Utf8StringRef>& fields() const noexcept;

    /// @brief 获取所有字段。
    /// @return 字段列表的可修改引用。
    std::vector<ca::str::Utf8StringRef>& fields() noexcept;

    /// @brief 在行尾追加字段。
    /// @param value 字段内容（Utf8StringRef，须已 intern 入池）。
    void push_back(ca::str::Utf8StringRef value);

private:
    std::vector<ca::str::Utf8StringRef> fields_;
};

/// @brief CSV 文档数据模型（Arena 架构）。
/// @details 文档由可选标题行和若干数据行组成。标题行不强制和记录行列数一致，
///          以便承载不规则 CSV；需要严格校验时可在上层业务中按 size() 判断。
class CsvDocument
{
public:
    CsvDocument();
    ~CsvDocument();

    CsvDocument(const CsvDocument&)            = delete;
    CsvDocument& operator=(const CsvDocument&) = delete;
    CsvDocument(CsvDocument&& other) noexcept;
    CsvDocument& operator=(CsvDocument&& other) noexcept;

    /// @brief 内部 arena（reader/用户需要把字段入池时用）。
    ca::str::Utf8StringArena& arena() noexcept;

    /// @brief 把字节区间入池为 Utf8StringRef（不校验 UTF-8，按原始字节保留）。
    /// @details CSV 字段可能是任意字节序列，用 intern_raw 保留原始字节。
    ///          返回的 ref 生命周期绑定本 document。
    ca::str::Utf8StringRef intern_field(const ca::u8* data, ca::usize byte_length);

    /// @brief 判断文档是否设置了标题行。
    /// @return 有标题行时返回 true。
    bool has_header() const noexcept;

    /// @brief 设置标题行（每个字段经 intern_field 入池）。
    /// @param header 标题字段（字节视图，会经 intern_raw 入池）。
    void set_header(const std::vector<std::string>& header);

    /// @brief 清除标题行。
    void clear_header() noexcept;

    /// @brief 获取标题行。
    /// @return 标题字段只读引用；未设置标题时返回空列表。
    const std::vector<ca::str::Utf8StringRef>& header() const noexcept;

    /// @brief 获取标题行。
    /// @return 标题字段可修改引用；调用后文档视为有标题行。
    std::vector<ca::str::Utf8StringRef>& header() noexcept;

    /// @brief 获取数据行。
    /// @return 数据行只读引用。
    const std::vector<CsvRow>& rows() const noexcept;

    /// @brief 获取数据行。
    /// @return 数据行可修改引用。
    std::vector<CsvRow>& rows() noexcept;

    /// @brief 追加一行数据。
    /// @param row 需要追加的行（字段 ref 须已 intern 入池）。
    void add_row(CsvRow row);

    /// @brief 清空标题行和所有数据行并释放 arena。所有 ref 失效。
    void clear() noexcept;

private:
    ca::str::Utf8StringArena            arena_;
    bool                                header_enabled_ = false;
    std::vector<ca::str::Utf8StringRef> header_;
    std::vector<CsvRow>                 rows_;
};

}   // namespace ca::csv
