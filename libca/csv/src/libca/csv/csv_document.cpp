#include "libca/csv/csv_document.hpp"

#include <utility>

namespace ca::csv {

// CsvRow 是 Utf8StringRef vector 的薄包装，拷贝/移动用默认即可（Utf8StringRef 可拷贝）。
CsvRow::CsvRow(std::vector<ca::str::Utf8StringRef> fields)
    : fields_(std::move(fields)) {}

ca::usize CsvRow::size() const noexcept {
    return fields_.size();
}

bool CsvRow::empty() const noexcept {
    return fields_.empty();
}

const ca::str::Utf8StringRef& CsvRow::operator[](ca::usize index) const noexcept {
    return fields_[index];
}

ca::str::Utf8StringRef& CsvRow::operator[](ca::usize index) noexcept {
    return fields_[index];
}

const std::vector<ca::str::Utf8StringRef>& CsvRow::fields() const noexcept {
    return fields_;
}

std::vector<ca::str::Utf8StringRef>& CsvRow::fields() noexcept {
    return fields_;
}

void CsvRow::push_back(ca::str::Utf8StringRef value) {
    fields_.push_back(value);
}

// ============================================================================
// CsvDocument 构造 / 析构 / 移动（arena_ 声明先于 header_/rows_，保证析构逆序：先树后池）
// ============================================================================

CsvDocument::CsvDocument() : arena_(), header_enabled_(false), header_(), rows_() {}

CsvDocument::~CsvDocument() = default;

CsvDocument::CsvDocument(CsvDocument&& other) noexcept
    : arena_(std::move(other.arena_)),
      header_enabled_(other.header_enabled_),
      header_(std::move(other.header_)),
      rows_(std::move(other.rows_)) {}

CsvDocument& CsvDocument::operator=(CsvDocument&& other) noexcept {
    if (this != &other) {
        arena_ = std::move(other.arena_);
        header_enabled_ = other.header_enabled_;
        header_ = std::move(other.header_);
        rows_ = std::move(other.rows_);
    }
    return *this;
}

ca::str::Utf8StringArena& CsvDocument::arena() noexcept { return arena_; }

ca::str::Utf8StringRef CsvDocument::intern_field(const ca::u8* data, ca::usize byte_length) {
    return arena_.intern_raw(data, byte_length);
}

bool CsvDocument::has_header() const noexcept {
    return header_enabled_;
}

void CsvDocument::set_header(const std::vector<std::string>& header) {
    header_.clear();
    header_.reserve(header.size());
    for (const auto& h : header) {
        header_.push_back(arena_.intern_raw(
            reinterpret_cast<const ca::u8*>(h.data()), h.size()));
    }
    header_enabled_ = true;
}

void CsvDocument::clear_header() noexcept {
    header_.clear();
    header_enabled_ = false;
}

const std::vector<ca::str::Utf8StringRef>& CsvDocument::header() const noexcept {
    return header_;
}

std::vector<ca::str::Utf8StringRef>& CsvDocument::header() noexcept {
    header_enabled_ = true;
    return header_;
}

const std::vector<CsvRow>& CsvDocument::rows() const noexcept {
    return rows_;
}

std::vector<CsvRow>& CsvDocument::rows() noexcept {
    return rows_;
}

void CsvDocument::add_row(CsvRow row) {
    rows_.push_back(std::move(row));
}

void CsvDocument::clear() noexcept {
    header_.clear();
    rows_.clear();
    header_enabled_ = false;
    arena_.clear();
}

}  // namespace ca::csv
