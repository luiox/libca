//
// @brief Utf8StringArena 实现
// @author Canrad
// @date 2026/05/31
//

#include "utf8_string_arena.hpp"
#include "utf8_util.hpp"

namespace ca::str {

// ============================================================================
// 构造 / 析构
// ============================================================================

Utf8StringArena::Utf8StringArena() noexcept
    : nextChunkIdx_(0) {
    allocChunk();
}

Utf8StringArena::~Utf8StringArena() {
    for (auto& c : chunks_)
        delete[] c.data;
}

Utf8StringArena::Utf8StringArena(Utf8StringArena&& other) noexcept
    : chunks_(std::move(other.chunks_))
    , entries_(std::move(other.entries_))
    , hashIndex_(std::move(other.hashIndex_))
    , nextChunkIdx_(other.nextChunkIdx_) {
    other.chunks_.clear();
    other.entries_.clear();
    other.hashIndex_.clear();
    other.nextChunkIdx_ = 0;
}

Utf8StringArena& Utf8StringArena::operator=(Utf8StringArena&& other) noexcept {
    if (this != &other) {
        for (auto& c : chunks_) delete[] c.data;
        chunks_ = std::move(other.chunks_);
        entries_ = std::move(other.entries_);
        hashIndex_ = std::move(other.hashIndex_);
        nextChunkIdx_ = other.nextChunkIdx_;
        other.chunks_.clear();
        other.entries_.clear();
        other.hashIndex_.clear();
        other.nextChunkIdx_ = 0;
    }
    return *this;
}


// ============================================================================
// 内部：块管理
// ============================================================================

void Utf8StringArena::allocChunk() {
    Chunk c;
    c.data     = new u8[kDefaultChunkSize];
    c.capacity = kDefaultChunkSize;
    c.used     = 0;
    chunks_.push_back(c);
    nextChunkIdx_ = chunks_.size() - 1;
}

u8* Utf8StringArena::allocInChunk(usize size) {
    // 当前块已满 → 新分配
    auto& cur = chunks_[nextChunkIdx_];
    if (cur.used + size > cur.capacity)
        allocChunk();

    auto& chunk = chunks_[nextChunkIdx_];
    u8* ptr = chunk.data + chunk.used;
    chunk.used += size;
    return ptr;
}


// ============================================================================
// Hash
// ============================================================================

usize Utf8StringArena::computeHash(const u8* data, usize byteLength) const noexcept {
    // FNV-1a
    usize h = 14695981039346656037ULL;
    for (usize i = 0; i < byteLength; ++i) {
        h ^= static_cast<usize>(data[i]);
        h *= 1099511628211ULL;
    }
    return h;
}


// ============================================================================
// intern
// ============================================================================

Utf8StringRef Utf8StringArena::intern(const u8* data, usize byteLength) {
    if (data == nullptr || byteLength == 0)
        return Utf8StringRef();

    auto hash = computeHash(data, byteLength);

    // 去重查找：同 hash 的条目逐个比较
    auto it = hashIndex_.find(hash);
    if (it != hashIndex_.end()) {
        for (auto idx : it->second) {
            auto& e = entries_[idx];
            if (e.byteLength == byteLength &&
                std::memcmp(chunks_[e.chunkIdx].data + e.chunkOffset, data, byteLength) == 0) {
                // 已存在，返回引用
                return Utf8StringRef(
                    chunks_[e.chunkIdx].data + e.chunkOffset,
                    e.byteLength, e.length);
            }
        }
    }

    // 计算码点个数
    usize cpCount = utf8CountCodePoints(data, byteLength);
    if (cpCount == 0) {
        // 非法 UTF-8 序列，返回空
        return Utf8StringRef();
    }

    // 分配到 chunk
    u8* dest = allocInChunk(byteLength);
    std::memcpy(dest, data, byteLength);

    // 记录条目
    Entry e;
    e.hash        = hash;
    e.byteLength  = byteLength;
    e.length      = cpCount;
    e.chunkIdx    = nextChunkIdx_;
    e.chunkOffset = static_cast<usize>(dest - chunks_[nextChunkIdx_].data);
    entries_.push_back(e);

    hashIndex_[hash].push_back(entries_.size() - 1);

    return Utf8StringRef(dest, byteLength, cpCount);
}

Utf8StringRef Utf8StringArena::intern(const char* cstr) {
    if (cstr == nullptr)
        return Utf8StringRef();
    return intern(reinterpret_cast<const u8*>(cstr), std::strlen(cstr));
}

Utf8StringRef Utf8StringArena::intern(const Utf8StringRef& str) {
    return intern(str.data(), str.byteLength());
}

Utf8StringRef Utf8StringArena::intern(const Utf8String& str) {
    return intern(str.data(), str.byteLength());
}


// ============================================================================
// 统计
// ============================================================================

usize Utf8StringArena::size() const noexcept {
    return entries_.size();
}

usize Utf8StringArena::totalBytes() const noexcept {
    usize total = 0;
    for (auto& c : chunks_)
        total += c.capacity;
    return total;
}

void Utf8StringArena::clear() noexcept {
    for (auto& c : chunks_)
        delete[] c.data;
    chunks_.clear();
    entries_.clear();
    hashIndex_.clear();
    nextChunkIdx_ = 0;
    allocChunk();
}

}  // namespace ca::str
