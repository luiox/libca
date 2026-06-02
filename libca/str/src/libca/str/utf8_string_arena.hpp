//
// @brief 追加式 UTF-8 字符串池 (Utf8StringArena)
// @author Canrad
// @date 2026/05/31
// @note 固定块链式扩展，内存不挪动，返回 Utf8StringRef 指向池内数据
//       池析构前引用稳定有效。无锁，非线程安全。
//
// 使用场景:
//   解析器、编译前端、配置加载 — 整批字符串共存，一起销毁
//   Utf8StringArena arena;
//   auto key = arena.intern("name");  // Utf8StringRef 指向 arena 内部
//   // arena 析构后 ref 失效
//

#ifndef LIBCA_STR_UTF8_STRING_ARENA_HPP
#define LIBCA_STR_UTF8_STRING_ARENA_HPP

#include "libca/core/datatype.hpp"
#include "utf8_string.hpp"

#include <cstddef>
#include <cstring>
#include <unordered_map>
#include <vector>

namespace ca::str {

class Utf8StringArena {
public:
    Utf8StringArena() noexcept;
    ~Utf8StringArena();

    // 不可拷贝
    Utf8StringArena(const Utf8StringArena&) = delete;
    Utf8StringArena& operator=(const Utf8StringArena&) = delete;

    // 可移动
    Utf8StringArena(Utf8StringArena&&) noexcept;
    Utf8StringArena& operator=(Utf8StringArena&&) noexcept;

    // ---- intern ----

    // 将 UTF-8 数据插入 arena，返回指向池内副本的引用
    Utf8StringRef intern(const u8* data, usize byteLength);
    Utf8StringRef intern(const char* cstr);
    Utf8StringRef intern(const Utf8StringRef& str);
    Utf8StringRef intern(const Utf8String& str);

    // ---- 统计 ----

    // 唯一字符串数量
    usize size() const noexcept;

    // 已分配的总字节数（含每个 chunk 剩余未用空间）
    usize totalBytes() const noexcept;

    // 重置，释放所有 chunk
    void clear() noexcept;

private:
    // 每个固定大小块
    struct Chunk {
        u8*   data;
        usize capacity;
        usize used;
    };

    // 条目元数据（用于去重查找）
    struct Entry {
        usize hash;
        usize byteLength;
        usize length;   // 码点个数
        usize chunkIdx;
        usize chunkOffset;
    };

    std::vector<Chunk>  chunks_;
    std::vector<Entry>  entries_;      // 条目元数据
    using HashIndex = std::unordered_map<usize, std::vector<usize>>;
    HashIndex           hashIndex_;    // hash → entry 下标列表

    usize nextChunkIdx_;  // 当前可写入的 chunk

    static constexpr usize kDefaultChunkSize = 64 * 1024;  // 64KB

    void  allocChunk();
    u8*   allocInChunk(usize size);
    usize computeHash(const u8* data, usize byteLength) const noexcept;
};

}  // namespace ca::str

#endif  // LIBCA_STR_UTF8_STRING_ARENA_HPP
