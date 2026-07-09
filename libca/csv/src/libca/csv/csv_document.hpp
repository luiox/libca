#pragma once

/// @file csv_document.hpp
/// @brief CSV 数据模型，提供 CsvRow 与 CsvDocument 两层结构。
/// @details 模型只表达表格数据本身，不保存 CSV 原始空白或换行风格；这些格式策略由
///          CsvReaderOptions 与 CsvWriterOptions 控制。字段内容按解析后的真实值保存，
///          写回时由 CsvWriter 负责按需加引号与转义。

#include "libca/core/datatype.hpp"

#include <string>
#include <vector>

namespace ca::csv {

/// @brief CSV 的一行记录。
/// @details CsvRow 是字段字符串的轻量包装，保留字段顺序，允许字段为空字符串。
class CsvRow {
public:
    /// @brief 构造空行。
    CsvRow() = default;

    /// @brief 从字段列表构造一行。
    /// @param fields 按 CSV 列顺序排列的字段。
    explicit CsvRow(std::vector<std::string> fields);

    /// @brief 返回字段数量。
    /// @return 当前行的列数。
    ca::usize size() const noexcept;

    /// @brief 判断是否没有字段。
    /// @return 没有字段时返回 true。
    bool empty() const noexcept;

    /// @brief 访问指定字段。
    /// @param index 字段下标。
    /// @return 字段引用。
    /// @warning 不做边界检查，调用方需保证 index 合法。
    const std::string& operator[](ca::usize index) const noexcept;

    /// @brief 访问指定字段。
    /// @param index 字段下标。
    /// @return 可修改字段引用。
    /// @warning 不做边界检查，调用方需保证 index 合法。
    std::string& operator[](ca::usize index) noexcept;

    /// @brief 获取所有字段。
    /// @return 字段列表的只读引用。
    const std::vector<std::string>& fields() const noexcept;

    /// @brief 获取所有字段。
    /// @return 字段列表的可修改引用。
    std::vector<std::string>& fields() noexcept;

    /// @brief 在行尾追加字段。
    /// @param value 字段内容。
    void push_back(std::string value);

private:
    std::vector<std::string> fields_;
};

/// @brief CSV 文档数据模型。
/// @details 文档由可选标题行和若干数据行组成。标题行不强制和记录行列数一致，
///          以便承载不规则 CSV；需要严格校验时可在上层业务中按 size() 判断。
class CsvDocument {
public:
    /// @brief 判断文档是否设置了标题行。
    /// @return 有标题行时返回 true。
    bool has_header() const noexcept;

    /// @brief 设置标题行。
    /// @param header 标题字段列表。
    void set_header(std::vector<std::string> header);

    /// @brief 清除标题行。
    void clear_header() noexcept;

    /// @brief 获取标题行。
    /// @return 标题字段列表的只读引用；未设置标题时返回空列表。
    const std::vector<std::string>& header() const noexcept;

    /// @brief 获取标题行。
    /// @return 标题字段列表的可修改引用；调用后文档视为有标题行。
    std::vector<std::string>& header() noexcept;

    /// @brief 获取数据行。
    /// @return 数据行只读引用。
    const std::vector<CsvRow>& rows() const noexcept;

    /// @brief 获取数据行。
    /// @return 数据行可修改引用。
    std::vector<CsvRow>& rows() noexcept;

    /// @brief 追加一行数据。
    /// @param row 需要追加的行。
    void add_row(CsvRow row);

    /// @brief 清空标题行和所有数据行。
    void clear() noexcept;

private:
    bool header_enabled_ = false;
    std::vector<std::string> header_;
    std::vector<CsvRow> rows_;
};

}  // namespace ca::csv
