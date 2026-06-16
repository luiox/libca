//
// @brief Utf8StringPool + Utf8StringPooledPtr 实现
// @author Canrad
// @date 2026/05/31
//

#include "utf8_string_pool.hpp"
#include "utf8_util.hpp"

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
        ++entry_->ref_count;
}

void Utf8StringPooledPtr::release() noexcept {
    if (entry_ && --entry_->ref_count == 0) {
        if (entry_->owner) entry_->owner->erase_entry(entry_);  // 真删
        entry_ = nullptr;
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

usize Utf8StringPooledPtr::byte_length() const noexcept {
    return entry_ ? entry_->byte_length : 0;
}

usize Utf8StringPooledPtr::length() const noexcept {
    return entry_ ? entry_->length : 0;
}

bool Utf8StringPooledPtr::is_empty() const noexcept {
    return entry_ == nullptr || entry_->byte_length == 0;
}

Utf8StringPooledPtr::operator bool() const noexcept {
    return entry_ != nullptr;
}

Utf8StringRef Utf8StringPooledPtr::ref() const noexcept {
    if (!entry_) return Utf8StringRef();
    return Utf8StringRef(entry_->data, entry_->byte_length, entry_->length);
}

bool Utf8StringPooledPtr::operator==(const Utf8StringPooledPtr& other) const noexcept {
    // 先指针：同池去重后同内容必同 entry（快路径）
    if (entry_ == other.entry_) return true;
    // 不同指针不代表不等：跨池同内容落不同地址，回退内容比较
    if (!entry_ || !other.entry_) return false;  // 一空一非空
    return ref() == other.ref();
}

bool Utf8StringPooledPtr::operator!=(const Utf8StringPooledPtr& other) const noexcept {
    return !(*this == other);
}


// ============================================================================
// Utf8StringPool
// ============================================================================

Utf8StringPool::Utf8StringPool() noexcept {}

Utf8StringPool::~Utf8StringPool() {
    // 真删模型下条目堆分配、由 hash_index_ 独占。析构释放所有条目
    // （含字节）。仍存活的 PooledPtr 之后 release 会发现 owner 已亡——
    // 故契约要求 Pool 必须 outlive 所有 PooledPtr（见 spec §5）。
    for (auto& [h, bucket] : hash_index_) {
        for (auto* ep : bucket) {
            delete[] ep->data;
            delete ep;
        }
    }
}

Utf8StringPool::Utf8StringPool(Utf8StringPool&& other) noexcept
    : hash_index_(std::move(other.hash_index_))
    , active_count_(other.active_count_)
    , total_bytes_(other.total_bytes_) {
    repoint_entries();              // 条目 owner 回指改向 this
    other.hash_index_.clear();
    other.active_count_ = 0;
    other.total_bytes_  = 0;
}

Utf8StringPool& Utf8StringPool::operator=(Utf8StringPool&& other) noexcept {
    if (this != &other) {
        for (auto& [h, bucket] : hash_index_) {   // 释放自身全部条目
            for (auto* ep : bucket) { delete[] ep->data; delete ep; }
        }
        hash_index_   = std::move(other.hash_index_);
        active_count_ = other.active_count_;
        total_bytes_  = other.total_bytes_;
        repoint_entries();
        other.hash_index_.clear();
        other.active_count_ = 0;
        other.total_bytes_  = 0;
    }
    return *this;
}

void Utf8StringPool::repoint_entries() noexcept {
    for (auto& [h, bucket] : hash_index_)
        for (auto* ep : bucket) ep->owner = this;
}

void Utf8StringPool::erase_entry(Utf8PoolEntry* entry) noexcept {
    if (!entry) return;
    auto it = hash_index_.find(entry->hash);
    if (it != hash_index_.end()) {
        auto& bucket = it->second;
        for (usize i = 0; i < bucket.size(); ++i) {
            if (bucket[i] == entry) {
                bucket[i] = bucket.back();   // 摘除：与末尾交换后弹出
                bucket.pop_back();
                break;
            }
        }
        if (bucket.empty()) hash_index_.erase(it);   // 空桶删 key
    }
    if (active_count_) --active_count_;
    if (total_bytes_ >= entry->byte_length) total_bytes_ -= entry->byte_length;
    delete[] entry->data;
    delete entry;
}

usize Utf8StringPool::compute_hash(const u8* data, usize byte_length) const noexcept {
    usize h = 14695981039346656037ULL;
    for (usize i = 0; i < byte_length; ++i) {
        h ^= static_cast<usize>(data[i]);
        h *= 1099511628211ULL;
    }
    return h;
}

Utf8StringPooledPtr Utf8StringPool::find(const Utf8StringRef& str) const {
    const u8* data = str.data();
    usize byte_length = str.byte_length();
    if (data == nullptr || byte_length == 0)
        return Utf8StringPooledPtr();

    auto hash = compute_hash(data, byte_length);
    auto it = hash_index_.find(hash);
    if (it != hash_index_.end()) {
        for (auto* ep : it->second) {
            if (ep->byte_length == byte_length &&
                std::memcmp(ep->data, data, byte_length) == 0) {
                ++ep->ref_count;   // 命中：返回持有句柄（与 intern 一致）
                return Utf8StringPooledPtr(ep);
            }
        }
    }
    return Utf8StringPooledPtr();  // 未命中
}

Utf8StringPooledPtr Utf8StringPool::intern(const u8* data, usize byte_length) {
    if (data == nullptr || byte_length == 0)
        return Utf8StringPooledPtr();

    auto hash = compute_hash(data, byte_length);

    // 去重查找：相同 hash 的条目中比较内容（真删后无墓碑，无需 alive 判断）
    auto it = hash_index_.find(hash);
    if (it != hash_index_.end()) {
        for (auto* ep : it->second) {
            if (ep->byte_length == byte_length &&
                std::memcmp(ep->data, data, byte_length) == 0) {
                ++ep->ref_count;
                return Utf8StringPooledPtr(ep);
            }
        }
    }

    usize cp_count = utf8_count_code_points(data, byte_length);
    if (cp_count == 0)
        return Utf8StringPooledPtr();  // 非法 UTF-8

    // 堆分配新条目（hash_index_ 独占所有权，指针稳定）
    auto* buf = new u8[byte_length];
    std::memcpy(buf, data, byte_length);
    auto* ep = new Utf8PoolEntry{buf, byte_length, cp_count, hash, 1, this};
    hash_index_[hash].push_back(ep);
    ++active_count_;
    total_bytes_ += byte_length;

    return Utf8StringPooledPtr(ep);
}

Utf8StringPooledPtr Utf8StringPool::intern(const char* cstr) {
    if (cstr == nullptr)
        return Utf8StringPooledPtr();
    return intern(reinterpret_cast<const u8*>(cstr), std::strlen(cstr));
}

Utf8StringPooledPtr Utf8StringPool::intern(const Utf8StringRef& str) {
    return intern(str.data(), str.byte_length());
}

usize Utf8StringPool::size() const noexcept {
    return active_count_;
}

usize Utf8StringPool::active_entries() const noexcept {
    return active_count_;
}

usize Utf8StringPool::total_bytes() const noexcept {
    return total_bytes_;
}

void Utf8StringPool::clear() noexcept {
    for (auto& [h, bucket] : hash_index_)
        for (auto* ep : bucket) { delete[] ep->data; delete ep; }
    hash_index_.clear();
    active_count_ = 0;
    total_bytes_  = 0;
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

bool operator==(const Utf8StringRef& lhs, const Utf8StringPooledPtr& rhs) noexcept {
    return lhs == rhs.ref();
}

bool operator!=(const Utf8StringRef& lhs, const Utf8StringPooledPtr& rhs) noexcept {
    return !(lhs == rhs.ref());
}

}  // namespace ca::str


// ============================================================================
// std::hash 特化
// ============================================================================

namespace std {

size_t hash<ca::str::Utf8StringPooledPtr>::operator()(
    const ca::str::Utf8StringPooledPtr& p) const noexcept {
    auto data = p.data();
    auto len  = p.byte_length();
    size_t h  = 14695981039346656037ULL;
    for (size_t i = 0; i < len; ++i) {
        h ^= static_cast<size_t>(data[i]);
        h *= 1099511628211ULL;
    }
    return h;
}

}  // namespace std
