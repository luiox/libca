#pragma once

/// @file ini_document.hpp
/// @brief INI 保格式数据模型。
/// @details IniDocument 以“行节点列表”为核心，保留注释、空行、section 顺序和未修改行的
///          原始文本。set/remove 等编辑操作只重建受影响的 key 或 section 行，Writer 会按
///          当前行节点顺序输出，从而满足配置文件读改写时尽量不扰动人工注释的需求。

#include "libca/core/datatype.hpp"
#include "libca/core/result.hpp"

#include <map>
#include <string>
#include <vector>

namespace ca::ini {

/// @brief INI 行类型。
enum class IniLineKind {
    Blank,    ///< 空行。
    Comment,  ///< 注释行。
    Section,  ///< section 声明行。
    KeyValue  ///< key/value 配置项行。
};

/// @brief INI 文档中的一行。
/// @details 该结构主要用于只读观察文档结构。原始行文本由模型内部维护，调用 set/remove
///          后会按保存的格式片段重建受影响行。
struct IniLine {
    IniLineKind kind = IniLineKind::Blank;  ///< 行类型。
    std::string section;                    ///< 所属 section；全局 key 为空字符串。
    std::string key;                        ///< KeyValue 行的 key。
    std::string value;                      ///< KeyValue 行的 value（不含行内注释）。
};

/// @brief INI 文档模型。
class IniDocument {
public:
    /// @brief 判断 section 是否存在。
    /// @param section section 名；空字符串表示全局区。
    /// @return 存在返回 true。
    bool has_section(const std::string& section) const noexcept;

    /// @brief 判断指定 key 是否存在。
    /// @param section section 名；空字符串表示全局区。
    /// @param key key 名。
    /// @return 存在返回 true。
    bool has(const std::string& section, const std::string& key) const noexcept;

    /// @brief 读取配置值。
    /// @param section section 名；空字符串表示全局区。
    /// @param key key 名。
    /// @return 成功返回 value；不存在返回错误说明。
    ca::Result<std::string, std::string> get(const std::string& section,
                                             const std::string& key) const;

    /// @brief 设置配置值；不存在时会插入到对应 section 末尾。
    /// @param section section 名；空字符串表示全局区。
    /// @param key key 名。
    /// @param value 新 value。
    void set(const std::string& section,
             const std::string& key,
             const std::string& value);

    /// @brief 移除整个 section 以及其下 key/value 行。
    /// @param section section 名；空字符串表示移除全局区 key/value 行。
    /// @return 实际删除了内容时返回 true。
    bool remove_section(const std::string& section);

    /// @brief 移除指定 key。
    /// @param section section 名；空字符串表示全局区。
    /// @param key key 名。
    /// @return key 存在并被删除时返回 true。
    bool remove(const std::string& section, const std::string& key);

    /// @brief 返回 section 列表，按文件中首次出现顺序排列。
    /// @return section 名列表；不包含全局空 section。
    std::vector<std::string> sections() const;

    /// @brief 返回指定 section 下的 key 列表，按文件中首次出现顺序排列并去重。
    /// @param section section 名；空字符串表示全局区。
    /// @return key 名列表；section 不存在时返回空列表。
    std::vector<std::string> keys(const std::string& section) const;

    /// @brief 返回解析出的行结构视图。
    /// @return 行结构列表。
    const std::vector<IniLine>& lines() const noexcept;

    /// @brief 清空文档。
    void clear() noexcept;

    /// @brief INI 行的内部保格式记录。
    /// @details 该结构公开是为了 Reader/Writer 在模块内协作；普通调用方通常只需要
    ///          `IniLine`、`get`、`set` 和 `remove` 这些高层接口。
    struct LineRecord {
        IniLine line;
        std::string raw;
        std::string line_ending;
        std::string key_prefix;
        std::string key_suffix;
        std::string separator;
        std::string value_prefix;
        std::string comment_suffix;
    };

private:
    friend class IniReader;
    friend class IniWriter;

    void add_record(LineRecord record);
    void rebuild_index();
    void rebuild_key_raw(ca::usize line_index);
    ca::usize find_insert_position(const std::string& section) const noexcept;
    std::string line_ending_for_new_line() const;

    std::vector<LineRecord> records_;
    std::vector<IniLine> public_lines_;
    std::map<std::string, ca::usize> section_index_;
    std::map<std::string, std::map<std::string, ca::usize>> key_index_;
    std::string default_line_ending_ = "\n";
};

}  // namespace ca::ini
