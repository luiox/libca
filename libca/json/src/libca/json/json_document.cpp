#include "libca/json/json_document.hpp"

#include <utility>

namespace ca::json {

JsonDocument::JsonDocument()
    : arena_()
    , root_()
{
    // root_ 默认构造即 null。
}

JsonDocument::~JsonDocument() = default;

JsonDocument::JsonDocument(JsonDocument&& other) noexcept
    : arena_(std::move(other.arena_))
    , root_(std::move(other.root_))
{}

JsonDocument& JsonDocument::operator=(JsonDocument&& other) noexcept
{
    if (this != &other) {
        arena_ = std::move(other.arena_);
        root_  = std::move(other.root_);
    }
    return *this;
}

JsonValue& JsonDocument::root() noexcept
{
    return root_;
}

const JsonValue& JsonDocument::root() const noexcept
{
    return root_;
}

ca::str::Utf8StringArena& JsonDocument::arena() noexcept
{
    return arena_;
}

void JsonDocument::clear() noexcept
{
    arena_.clear();
    root_ = JsonValue::make_null();
}

}   // namespace ca::json
