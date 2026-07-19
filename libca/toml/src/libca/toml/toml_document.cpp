#include "libca/toml/toml_document.hpp"

#include <utility>

namespace ca::toml {

TomlDocument::TomlDocument() : arena_(), root_() {
    // root_ 默认构造即 Table。
}

TomlDocument::~TomlDocument() = default;

TomlDocument::TomlDocument(TomlDocument&& other) noexcept
    : arena_(std::move(other.arena_)), root_(std::move(other.root_)) {}

TomlDocument& TomlDocument::operator=(TomlDocument&& other) noexcept {
    if (this != &other) {
        arena_ = std::move(other.arena_);
        root_ = std::move(other.root_);
    }
    return *this;
}

TomlValue& TomlDocument::root() noexcept { return root_; }

const TomlValue& TomlDocument::root() const noexcept { return root_; }

ca::str::Utf8StringArena& TomlDocument::arena() noexcept { return arena_; }

void TomlDocument::clear() noexcept {
    arena_.clear();
    root_ = TomlValue::make_table();
}

}  // namespace ca::toml
