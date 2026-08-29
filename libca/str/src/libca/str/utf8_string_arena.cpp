//
// @brief Utf8StringArena 实现
// @author Canrad
// @date 2026/05/31
//

#include "utf8_string_arena.hpp"
#include "utf8_util.hpp"

#include <cstdint>

namespace ca::str {

// ============================================================================
// 构造 / 析构
// ============================================================================

Utf8StringArena::Utf8StringArena() noexcept
    : next_chunk_idx_(0) {
    alloc_chunk();
}

Utf8StringArena::~Utf8StringArena() {
    for (auto& c : chunks_)
        delete[] c.data;
}

Utf8StringArena::Utf8StringArena(Utf8StringArena&& other) noexcept
    : chunks_(std::move(other.chunks_))
    , entries_(std::move(other.entries_))
    , hash_index_(std::move(other.hash_index_))
    , next_chunk_idx_(other.next_chunk_idx_) {
    other.chunks_.clear();
    other.entries_.clear();
    other.hash_index_.clear();
    other.next_chunk_idx_ = 0;
}

Utf8StringArena& Utf8StringArena::operator=(Utf8StringArena&& other) noexcept {
    if (this != &other) {
        for (auto& c : chunks_) delete[] c.data;
        chunks_ = std::move(other.chunks_);
        entries_ = std::move(other.entries_);
        hash_index_ = std::move(other.hash_index_);
        next_chunk_idx_ = other.next_chunk_idx_;
        other.chunks_.clear();
        other.entries_.clear();
        other.hash_index_.clear();
        other.next_chunk_idx_ = 0;
    }
    return *this;
}


// ============================================================================
// 内部：块管理
// ============================================================================

void Utf8StringArena::alloc_chunk(usize min_capacity) {
    usize capacity = DEFAULT_CHUNK_SIZE;
    if (capacity < min_capacity)
        capacity = min_capacity;

    Chunk c;
    c.data     = new u8[capacity];
    c.capacity = capacity;
    c.used     = 0;
    chunks_.push_back(c);
    next_chunk_idx_ = chunks_.size() - 1;
}

u8* Utf8StringArena::alloc_in_chunk(usize size) {
    if (chunks_.empty())
        alloc_chunk(size);

    // 当前块已满 → 新分配
    auto& cur = chunks_[next_chunk_idx_];
    if (size > cur.capacity - cur.used)
        alloc_chunk(size);

    auto& chunk = chunks_[next_chunk_idx_];
    u8* ptr = chunk.data + chunk.used;
    chunk.used += size;
    return ptr;
}


// ============================================================================
// Hash
// ============================================================================

usize Utf8StringArena::compute_hash(const u8* data, usize byte_length) const noexcept {
    // FNV-1a
    usize h = 14695981039346656037ULL;
    for (usize i = 0; i < byte_length; ++i) {
        h ^= static_cast<usize>(data[i]);
        h *= 1099511628211ULL;
    }
    return h;
}


// ============================================================================
// intern
// ============================================================================

Utf8StringRef Utf8StringArena::intern(const u8* data, usize byte_length) {
    if (data == nullptr || byte_length == 0)
        return Utf8StringRef();

    auto hash = compute_hash(data, byte_length);

    // 去重查找：同 hash 的条目逐个比较
    auto it = hash_index_.find(hash);
    if (it != hash_index_.end()) {
        for (auto idx : it->second) {
            auto& e = entries_[idx];
            if (e.byte_length == byte_length &&
                std::memcmp(chunks_[e.chunk_idx].data + e.chunk_offset, data, byte_length) == 0) {
                // 已存在，返回引用
                return Utf8StringRef(
                    chunks_[e.chunk_idx].data + e.chunk_offset,
                    e.byte_length, e.length);
            }
        }
    }

    // 计算码点个数
    usize cp_count = utf8_count_code_points(data, byte_length);
    if (cp_count == 0) {
        // 非法 UTF-8 序列，返回空
        return Utf8StringRef();
    }

    // 分配到 chunk
    u8* dest = alloc_in_chunk(byte_length);
    std::memcpy(dest, data, byte_length);

    // 记录条目
    Entry e;
    e.hash         = hash;
    e.byte_length  = byte_length;
    e.length       = cp_count;
    e.chunk_idx    = next_chunk_idx_;
    e.chunk_offset = static_cast<usize>(dest - chunks_[next_chunk_idx_].data);
    entries_.push_back(e);

    hash_index_[hash].push_back(entries_.size() - 1);

    return Utf8StringRef(dest, byte_length, cp_count);
}

Utf8StringRef Utf8StringArena::intern_raw(const u8* data, usize byte_length) {
    // 不校验 UTF-8：用于字节流载体（非合法 UTF-8 但 JVM modified UTF-8 可往返）。
    // 码点数 length 取保守值 byte_length（上界）。
    if (data == nullptr || byte_length == 0)
        return Utf8StringRef();

    auto hash = compute_hash(data, byte_length);

    // 去重查找（与 intern 相同）
    auto it = hash_index_.find(hash);
    if (it != hash_index_.end()) {
        for (auto idx : it->second) {
            auto& e = entries_[idx];
            if (e.byte_length == byte_length &&
                std::memcmp(chunks_[e.chunk_idx].data + e.chunk_offset, data, byte_length) == 0) {
                return Utf8StringRef(
                    chunks_[e.chunk_idx].data + e.chunk_offset,
                    e.byte_length, e.length);
            }
        }
    }

    // 跳过 utf8_count_code_points，length 取 byte_length
    u8* dest = alloc_in_chunk(byte_length);
    std::memcpy(dest, data, byte_length);

    Entry e;
    e.hash         = hash;
    e.byte_length  = byte_length;
    e.length       = byte_length;
    e.chunk_idx    = next_chunk_idx_;
    e.chunk_offset = static_cast<usize>(dest - chunks_[next_chunk_idx_].data);
    entries_.push_back(e);

    hash_index_[hash].push_back(entries_.size() - 1);

    return Utf8StringRef(dest, byte_length, byte_length);
}

Utf8StringRef Utf8StringArena::intern(const char* cstr) {
    if (cstr == nullptr)
        return Utf8StringRef();
    return intern(reinterpret_cast<const u8*>(cstr), std::strlen(cstr));
}

Utf8StringRef Utf8StringArena::intern(const Utf8StringRef& str) {
    return intern(str.data(), str.byte_length());
}

Utf8StringRef Utf8StringArena::intern(const Utf8String& str) {
    return intern(str.data(), str.byte_length());
}

Utf8StringRef Utf8StringArena::intern(std::string_view sv) {
    return intern(reinterpret_cast<const u8*>(sv.data()), sv.size());
}


// ============================================================================
// 归属检查
// ============================================================================

bool Utf8StringArena::owns(const Utf8StringRef& ref) const noexcept {
    if (ref.data() == nullptr || ref.byte_length() == 0)
        return false;

    const auto begin = reinterpret_cast<std::uintptr_t>(ref.data());
    const auto end = begin + ref.byte_length();
    if (end < begin)
        return false;

    for (const auto& c : chunks_) {
        const auto chunk_begin = reinterpret_cast<std::uintptr_t>(c.data);
        const auto chunk_end = chunk_begin + c.used;
        if (chunk_end < chunk_begin)
            continue;
        if (begin >= chunk_begin && end <= chunk_end)
            return true;
    }
    return false;
}


// ============================================================================
// 统计
// ============================================================================

usize Utf8StringArena::size() const noexcept {
    return entries_.size();
}

usize Utf8StringArena::total_bytes() const noexcept {
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
    hash_index_.clear();
    next_chunk_idx_ = 0;
    alloc_chunk();
}

}  // namespace ca::str
