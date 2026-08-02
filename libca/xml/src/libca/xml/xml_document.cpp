#include "libca/xml/xml_document.hpp"

#include <utility>

namespace ca::xml {

XmlDocument::XmlDocument() = default;

XmlDocument::~XmlDocument() = default;

XmlDocument::XmlDocument(XmlDocument&& other) noexcept
    : arena_(std::move(other.arena_)),
      declaration_(other.declaration_),
      prolog_(std::move(other.prolog_)),
      epilog_(std::move(other.epilog_)),
      root_(std::move(other.root_)) {
    other.declaration_ = XmlDeclaration{};
}

XmlDocument& XmlDocument::operator=(XmlDocument&& other) noexcept {
    if (this != &other) {
        arena_ = std::move(other.arena_);
        declaration_ = other.declaration_;
        prolog_ = std::move(other.prolog_);
        epilog_ = std::move(other.epilog_);
        root_ = std::move(other.root_);
        other.declaration_ = XmlDeclaration{};
    }
    return *this;
}

XmlNode& XmlDocument::root() noexcept { return root_; }
const XmlNode& XmlDocument::root() const noexcept { return root_; }

XmlDeclaration& XmlDocument::declaration() noexcept { return declaration_; }
const XmlDeclaration& XmlDocument::declaration() const noexcept { return declaration_; }

std::vector<XmlNode>& XmlDocument::prolog() noexcept { return prolog_; }
const std::vector<XmlNode>& XmlDocument::prolog() const noexcept { return prolog_; }

std::vector<XmlNode>& XmlDocument::epilog() noexcept { return epilog_; }
const std::vector<XmlNode>& XmlDocument::epilog() const noexcept { return epilog_; }

ca::str::Utf8StringArena& XmlDocument::arena() noexcept { return arena_; }

void XmlDocument::clear() noexcept {
    arena_.clear();
    declaration_ = XmlDeclaration{};
    prolog_.clear();
    epilog_.clear();
    root_ = XmlNode();
}

}  // namespace ca::xml
