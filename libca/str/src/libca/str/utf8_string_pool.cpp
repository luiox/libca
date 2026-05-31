//
// @brief Utf8StringPool + Utf8StringPooledPtr 实现
// @author Canrad
// @date 2026/05/31
//

#include "utf8_string_pool.hpp"

namespace ca::str {

// ============================================================================
// Utf8StringPooledPtr
// ============================================================================

Utf8StringPooledPtr::Utf8StringPooledPtr() noexcept
    : entry_(nullptr) {}

Utf8StringPooledPtr::Utf8StringPooledPtr(Utf8PoolEntry* entry) noexcept
    : entry_(entry) {}

void Utf8StringPooledPtr::acquire() noexcept {
    if (entry_)
        ++entry_->refCount;
}

void Utf8StringPooledPtr::release() noexcept {
    if (entry_ && --entry_->refCount == 0) {
        entry_->alive = false;
        delete[] entry_->data;
        entry_->data = nullptr;
    }
}

Utf8StringPooledPtr::Utf8StringPooledPtr(const Utf8StringPooledPtr& other) noexcept
    : entry_(other.entry_) {
    acquire();
}

Utf8StringPooledPtr::Utf8StringPooledPtr(Utf8StringPooledPtr&& other) noexcept
    : entry_(other.entry_) {
    other.entry_ = nullptr;
}

Utf8StringPooledPtr::~Utf8StringPooledPtr() {
    release();
}

Utf8StringPooledPtr& Utf8StringPooledPtr::operator=(const Utf8StringPooledPtr& other) noexcept {
    if (this != &other) {
        release();
        entry_ = other.entry_;
        acquire();
    }
    return *this;
}

Utf8StringPooledPtr& Utf8StringPooledPtr::operator=(Utf8StringPooledPtr&& other) noexcept {
    if (this != &other) {
        release();
        entry_ = other.entry_;
        other.entry_ = nullptr;
    }
    return *this;
}

const u8* Utf8StringPooledPtr::data() const noexcept {
    return entry_ ? entry_->data : nullptr;
}

usize Utf8StringPooledPtr::byteLength() const noexcept {
    return entry_ ? entry_->byteLength : 0;
}

usize Utf8StringPooledPtr::length() const noexcept {
    return entry_ ? entry_->length : 0;
}

bool Utf8StringPooledPtr::isEmpty() const noexcept {
    return entry_ == nullptr || entry_->byteLength == 0;
}

Utf8StringPooledPtr::operator bool() const noexcept {
    return entry_ != nullptr;
}

Utf8StringRef Utf8StringPooledPtr::ref() const noexcept {
    if (!entry_) return Utf8StringRef();
    return Utf8StringRef(entry_->data, entry_->byteLength, entry_->length);
}

bool Utf8StringPooledPtr::operator==(const Utf8StringPooledPtr& other) const noexcept {
    return entry_ == other.entry_;
}

bool Utf8StringPooledPtr::operator!=(const Utf8StringPooledPtr& other) const noexcept {
    return entry_ != other.entry_;
}


// ============================================================================
// Utf8StringPool
// ============================================================================

Utf8StringPool::Utf8StringPool() noexcept {}

Utf8StringPool::~Utf8StringPool() {
    // 释放所有 dead entry 的数据内存
    // alive entry 的 data 仍有效，由 PooledPtr 析构时释放
    for (auto& e : entries_) {
        if (!e.alive && e.data) {
            delete[] e.data;
            e.data = nullptr;
        }
    }
}

Utf8StringPool::Utf8StringPool(Utf8StringPool&& other) noexcept
    : entries_(std::move(other.entries_))
    , hashIndex_(std::move(other.hashIndex_)) {
    other.entries_.clear();
    other.hashIndex_.clear();
}

Utf8StringPool& Utf8StringPool::operator=(Utf8StringPool&& other) noexcept {
    if (this != &other) {
        // 释放自身 dead 数据
        for (auto& e : entries_) {
            if (!e.alive && e.data) {
                delete[] e.data;
                e.data = nullptr;
            }
        }
        entries_ = std::move(other.entries_);
        hashIndex_ = std::move(other.hashIndex_);
        other.entries_.clear();
        other.hashIndex_.clear();
    }
    return *this;
}

usize Utf8StringPool::computeHash(const u8* data, usize byteLength) const noexcept {
    usize h = 14695981039346656037ULL;
    for (usize i = 0; i < byteLength; ++i) {
        h ^= static_cast<usize>(data[i]);
        h *= 1099511628211ULL;
    }
    return h;
}

Utf8StringPooledPtr Utf8StringPool::intern(const u8* data, usize byteLength) {
    if (data == nullptr || byteLength == 0)
        return Utf8StringPooledPtr();

    auto hash = computeHash(data, byteLength);

    // 去重查找：相同 hash 的活跃条目中比较
    auto it = hashIndex_.find(hash);
    if (it != hashIndex_.end()) {
        for (auto* ep : it->second) {
            if (ep->alive && ep->byteLength == byteLength &&
                std::memcmp(ep->data, data, byteLength) == 0) {
                // 找到已存在的条目，递增 refCount
                ++ep->refCount;
                return Utf8StringPooledPtr(ep);
            }
        }
    }

    // 计算码点个数
    usize cpCount = utf8CountCodePoints(data, byteLength);
    if (cpCount == 0)
        return Utf8StringPooledPtr();  // 非法 UTF-8

    // 分配新条目
    auto* buf = new u8[byteLength];
    std::memcpy(buf, data, byteLength);

    entries_.push_back({buf, byteLength, cpCount, hash, 1, true});
    auto* ep = &entries_.back();
    hashIndex_[hash].push_back(ep);

    return Utf8StringPooledPtr(ep);
}

Utf8StringPooledPtr Utf8StringPool::intern(const char* cstr) {
    if (cstr == nullptr)
        return Utf8StringPooledPtr();
    return intern(reinterpret_cast<const u8*>(cstr), std::strlen(cstr));
}

Utf8StringPooledPtr Utf8StringPool::intern(const Utf8StringRef& str) {
    return intern(str.data(), str.byteLength());
}

usize Utf8StringPool::size() const noexcept {
    return entries_.size();
}

usize Utf8StringPool::activeEntries() const noexcept {
    usize count = 0;
    for (auto& e : entries_) {
        if (e.alive) ++count;
    }
    return count;
}

usize Utf8StringPool::totalBytes() const noexcept {
    usize total = 0;
    for (auto& e : entries_) {
        if (e.alive)
            total += e.byteLength;
    }
    return total;
}

void Utf8StringPool::clear() noexcept {
    for (auto& e : entries_) {
        if (e.data) {
            delete[] e.data;
            e.data = nullptr;
        }
    }
    entries_.clear();
    hashIndex_.clear();
}


// ============================================================================
// 非成员比较
// ============================================================================

bool operator==(const Utf8StringPooledPtr& lhs, const Utf8StringRef& rhs) noexcept {
    return lhs.ref() == rhs;
}

bool operator!=(const Utf8StringPooledPtr& lhs, const Utf8StringRef& rhs) noexcept {
    return !(lhs.ref() == rhs);
}

}  // namespace ca::str


// ============================================================================
// std::hash 特化
// ============================================================================

namespace std {

size_t hash<ca::str::Utf8StringPooledPtr>::operator()(
    const ca::str::Utf8StringPooledPtr& p) const noexcept {
    auto data = p.data();
    auto len  = p.byteLength();
    size_t h  = 14695981039346656037ULL;
    for (size_t i = 0; i < len; ++i) {
        h ^= static_cast<size_t>(data[i]);
        h *= 1099511628211ULL;
    }
    return h;
}

}  // namespace std
