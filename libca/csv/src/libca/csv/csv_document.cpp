#include "libca/csv/csv_document.hpp"

namespace ca::csv {

CsvRow::CsvRow(std::vector<std::string> fields)
    : fields_(std::move(fields)) {}

ca::usize CsvRow::size() const noexcept {
    return fields_.size();
}

bool CsvRow::empty() const noexcept {
    return fields_.empty();
}

const std::string& CsvRow::operator[](ca::usize index) const noexcept {
    return fields_[index];
}

std::string& CsvRow::operator[](ca::usize index) noexcept {
    return fields_[index];
}

const std::vector<std::string>& CsvRow::fields() const noexcept {
    return fields_;
}

std::vector<std::string>& CsvRow::fields() noexcept {
    return fields_;
}

void CsvRow::push_back(std::string value) {
    fields_.push_back(std::move(value));
}

bool CsvDocument::has_header() const noexcept {
    return header_enabled_;
}

void CsvDocument::set_header(std::vector<std::string> header) {
    header_ = std::move(header);
    header_enabled_ = true;
}

void CsvDocument::clear_header() noexcept {
    header_.clear();
    header_enabled_ = false;
}

const std::vector<std::string>& CsvDocument::header() const noexcept {
    return header_;
}

std::vector<std::string>& CsvDocument::header() noexcept {
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
}

}  // namespace ca::csv
