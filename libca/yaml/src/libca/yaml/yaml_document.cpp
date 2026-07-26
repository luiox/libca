#include "libca/yaml/yaml_document.hpp"

#include <utility>

namespace ca::yaml {

YamlDocument::YamlDocument() : arena_(), root_() {
    // root_ 默认构造即 Null。
}

YamlDocument::~YamlDocument() = default;

YamlDocument::YamlDocument(YamlDocument&& other) noexcept
    : arena_(std::move(other.arena_)), root_(std::move(other.root_)) {}

YamlDocument& YamlDocument::operator=(YamlDocument&& other) noexcept {
    if (this != &other) {
        arena_ = std::move(other.arena_);
        root_ = std::move(other.root_);
    }
    return *this;
}

YamlValue& YamlDocument::root() noexcept { return root_; }

const YamlValue& YamlDocument::root() const noexcept { return root_; }

ca::str::Utf8StringArena& YamlDocument::arena() noexcept { return arena_; }

void YamlDocument::clear() noexcept {
    arena_.clear();
    root_ = YamlValue::make_null();
}

}  // namespace ca::yaml
