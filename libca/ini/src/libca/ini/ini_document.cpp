#include "libca/ini/ini_document.hpp"

#include <algorithm>
#include <utility>

namespace ca::ini {

bool IniDocument::has_section(const std::string& section) const noexcept {
    if (section.empty()) {
        return key_index_.find(section) != key_index_.end();
    }
    return section_index_.find(section) != section_index_.end();
}

bool IniDocument::has(const std::string& section, const std::string& key) const noexcept {
    auto section_it = key_index_.find(section);
    if (section_it == key_index_.end()) {
        return false;
    }
    return section_it->second.find(key) != section_it->second.end();
}

ca::Result<std::string, std::string> IniDocument::get(
    const std::string& section,
    const std::string& key) const {
    auto section_it = key_index_.find(section);
    if (section_it == key_index_.end()) {
        return ca::Err(std::string("INI section not found: ") + section);
    }
    auto key_it = section_it->second.find(key);
    if (key_it == section_it->second.end()) {
        return ca::Err(std::string("INI key not found: ") + key);
    }
    return ca::Ok(records_[key_it->second].line.value);
}

void IniDocument::set(const std::string& section,
                      const std::string& key,
                      const std::string& value) {
    // 已存在的 key：只改 value，复用解析时保存的格式片段（key 前缩进、分隔符、
    // 行内注释）重建该行，不扰动文件其余部分。
    auto section_it = key_index_.find(section);
    if (section_it != key_index_.end()) {
        auto key_it = section_it->second.find(key);
        if (key_it != section_it->second.end()) {
            auto& record = records_[key_it->second];
            record.line.value = value;
            rebuild_key_raw(key_it->second);
            public_lines_[key_it->second] = record.line;
            return;
        }
    }

    const auto newline = line_ending_for_new_line();
    // 新 key 落在尚不存在的 section：先建 section 头。若上一行非空则补一个空行分隔，
    // 保持人工配置文件常见的 section 间空行习惯。
    if (!section.empty() && section_index_.find(section) == section_index_.end()) {
        if (!records_.empty() && !records_.back().raw.empty()) {
            LineRecord blank;
            blank.line.kind = IniLineKind::Blank;
            blank.line_ending = newline;
            records_.push_back(std::move(blank));
        }

        LineRecord section_record;
        section_record.line.kind = IniLineKind::Section;
        section_record.line.section = section;
        section_record.raw = std::string("[") + section + "]";
        section_record.line_ending = newline;
        records_.push_back(std::move(section_record));
        rebuild_index();
    }

    // 新增 key 行用统一的 "key = value" 格式；格式片段留默认值，后续编辑沿用。
    LineRecord key_record;
    key_record.line.kind = IniLineKind::KeyValue;
    key_record.line.section = section;
    key_record.line.key = key;
    key_record.line.value = value;
    key_record.key_suffix = " ";
    key_record.separator = "=";
    key_record.value_prefix = " ";
    key_record.line_ending = newline;
    key_record.raw = key + " = " + value;

    auto pos = find_insert_position(section);
    records_.insert(records_.begin() + static_cast<std::ptrdiff_t>(pos),
                    std::move(key_record));
    rebuild_index();
}

bool IniDocument::remove_section(const std::string& section) {
    bool removed = false;
    if (section.empty()) {
        records_.erase(
            std::remove_if(records_.begin(), records_.end(), [&](const LineRecord& record) {
                const bool should_remove =
                    record.line.kind == IniLineKind::KeyValue && record.line.section.empty();
                removed = removed || should_remove;
                return should_remove;
            }),
            records_.end());
        if (removed) {
            rebuild_index();
        }
        return removed;
    }

    auto it = section_index_.find(section);
    if (it == section_index_.end()) {
        return false;
    }

    const ca::usize start = it->second;
    ca::usize end = start + 1;
    while (end < records_.size() && records_[end].line.kind != IniLineKind::Section) {
        ++end;
    }
    // 回收 section 末尾紧跟的空行/注释，避免删除后留下孤立空行。
    if (end < records_.size()) {
        while (end > start + 1) {
            const auto kind = records_[end - 1].line.kind;
            if (kind != IniLineKind::Blank && kind != IniLineKind::Comment) {
                break;
            }
            --end;
        }
    }
    records_.erase(records_.begin() + static_cast<std::ptrdiff_t>(start),
                   records_.begin() + static_cast<std::ptrdiff_t>(end));
    rebuild_index();
    return true;
}

bool IniDocument::remove(const std::string& section, const std::string& key) {
    auto section_it = key_index_.find(section);
    if (section_it == key_index_.end()) {
        return false;
    }
    auto key_it = section_it->second.find(key);
    if (key_it == section_it->second.end()) {
        return false;
    }
    records_.erase(records_.begin() + static_cast<std::ptrdiff_t>(key_it->second));
    rebuild_index();
    return true;
}

std::vector<std::string> IniDocument::sections() const {
    std::vector<std::string> result;
    for (const auto& record : records_) {
        if (record.line.kind == IniLineKind::Section) {
            result.push_back(record.line.section);
        }
    }
    return result;
}

std::vector<std::string> IniDocument::keys(const std::string& section) const {
    std::vector<std::string> result;
    std::map<std::string, bool> seen;
    for (const auto& record : records_) {
        if (record.line.kind == IniLineKind::KeyValue &&
            record.line.section == section &&
            seen.find(record.line.key) == seen.end()) {
            seen[record.line.key] = true;
            result.push_back(record.line.key);
        }
    }
    return result;
}

const std::vector<IniLine>& IniDocument::lines() const noexcept {
    return public_lines_;
}

void IniDocument::clear() noexcept {
    records_.clear();
    public_lines_.clear();
    section_index_.clear();
    key_index_.clear();
}

void IniDocument::add_record(LineRecord record) {
    if (default_line_ending_ == "\n" && !record.line_ending.empty()) {
        default_line_ending_ = record.line_ending;
    }
    records_.push_back(std::move(record));
}

void IniDocument::rebuild_index() {
    public_lines_.clear();
    section_index_.clear();
    key_index_.clear();
    public_lines_.reserve(records_.size());

    for (ca::usize i = 0; i < records_.size(); ++i) {
        const auto& line = records_[i].line;
        public_lines_.push_back(line);
        if (line.kind == IniLineKind::Section) {
            section_index_[line.section] = i;
        } else if (line.kind == IniLineKind::KeyValue) {
            key_index_[line.section][line.key] = i;
        }
    }
}

void IniDocument::rebuild_key_raw(ca::usize line_index) {
    auto& record = records_[line_index];
    record.raw = record.key_prefix + record.line.key + record.key_suffix +
                 record.separator + record.value_prefix + record.line.value +
                 record.comment_suffix;
}

ca::usize IniDocument::find_insert_position(const std::string& section) const noexcept {
    if (section.empty()) {
        ca::usize pos = 0;
        while (pos < records_.size() &&
               records_[pos].line.kind != IniLineKind::Section) {
            ++pos;
        }
        return pos;
    }

    auto section_it = section_index_.find(section);
    if (section_it == section_index_.end()) {
        return records_.size();
    }
    ca::usize pos = section_it->second + 1;
    while (pos < records_.size() &&
           records_[pos].line.kind != IniLineKind::Section) {
        ++pos;
    }
    return pos;
}

std::string IniDocument::line_ending_for_new_line() const {
    return default_line_ending_.empty() ? std::string("\n") : default_line_ending_;
}

}  // namespace ca::ini
