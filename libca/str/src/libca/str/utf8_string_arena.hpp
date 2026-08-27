/// @file utf8_string_arena.hpp
/// @brief 追加式 UTF-8 字符串池 Utf8StringArena：复制输入、去重、返回指向池内数据的 Utf8StringRef。
/// @author Canrad
/// @date 2026/05/31
/// @note 固定块链式扩展，内存不挪动。无锁，**非线程安全**。
///       选型：一批字符串有共同死亡点（整批共存、一起销毁）用 arena；寿命各异用 Utf8StringPool。
/// @warning **返回的 Utf8StringRef 生命周期绑定 arena**：arena 析构或 clear() 后全部失效；
///          移动赋值会释放目标旧 chunk，使其旧 ref 失效。
/// @code
///   Utf8StringArena arena;
///   auto key = arena.intern("name");  // ref 指向 arena 内部
///   // arena 析构后 key 失效
/// @endcode

#pragma once

#include "libca/core/datatype.hpp"
#include "utf8_string.hpp"

#include <cstddef>
#include <cstring>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace ca::str {

/// @brief 追加式去重 UTF-8 字符串池。详见文件头说明。
class Utf8StringArena {
public:
    /// 构造空池并分配初始 chunk。
    Utf8StringArena() noexcept;
    /// 释放所有 chunk（此后所有返回的 ref 失效）。
    ~Utf8StringArena();

    Utf8StringArena(const Utf8StringArena&) = delete;
    Utf8StringArena& operator=(const Utf8StringArena&) = delete;

    Utf8StringArena(Utf8StringArena&&) noexcept;
    Utf8StringArena& operator=(Utf8StringArena&&) noexcept;

    // ---- intern：复制入池、去重，返回指向池内副本的视图 ----

    /// @brief 校验 UTF-8 并复制入池，内容去重。空或非法输入返回空视图。
    Utf8StringRef intern(const u8* data, usize byte_length);
    /// intern C 字符串；空指针返回空视图。
    Utf8StringRef intern(const char* cstr);
    /// intern 视图内容。
    Utf8StringRef intern(const Utf8StringRef& str);
    /// intern 拥有字符串内容。
    Utf8StringRef intern(const Utf8String& str);
    /// intern 标准库视图内容（std::string 拼装产物一次性入池的规范入口，
    /// 替代 `intern(sv.c_str())` 舞步；空视图返回空视图）。
    Utf8StringRef intern(std::string_view sv);

    /// @brief 不校验 UTF-8，按原始字节复制入池（码点数取保守值 = 字节长度）。
    ///
    /// 用于字节流载体（如 JVM 字节码混淆的密文载体）：这类数据不是合法 UTF-8，
    /// 但 JVM 的 modified UTF-8 编解码能 1:1 往返 0x00-0xFF 字节，ClassWriter 会按
    /// modified UTF-8 重新编码写入常量池。本方法跳过校验，让任意字节入池。
    /// @note 码点数 length 取 byte_length（保守上界），调用方若需精确码点数应自行计算。
    Utf8StringRef intern_raw(const u8* data, usize byte_length);

    // ---- 统计 ----

    /// 唯一字符串数量。
    usize size() const noexcept;

    /// 已分配 chunk 总容量（含每个 chunk 未用空间）。
    usize total_bytes() const noexcept;

    /// 释放所有 chunk 和索引，回到空池（此后所有返回的 ref 失效）。
    void clear() noexcept;

    // ---- 归属检查 ----

    /// @brief 视图数据是否指向本 arena 拥有的 chunk（debug 断言用，best-effort）。
    ///
    /// 指针范围判定：落入任一 chunk 的 `[data, data + used)` 即归本池。
    /// 空视图返回 false。用途：上层容器（如 TreeContext）在 debug 构建断言
    /// 挂载的字符串视图确属本池，拦截「视图指向临时对象/别的池」类悬垂。
    /// @note clear()/移动赋值后旧 ref 悬垂，本方法无法侦测——语义前提是
    ///       「ref 诞生后 arena 未回收」。
    bool owns(const Utf8StringRef& ref) const noexcept;

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
        usize byte_length;
        usize length;   // 码点个数
        usize chunk_idx;
        usize chunk_offset;
    };

    std::vector<Chunk>  chunks_;
    std::vector<Entry>  entries_;      // 条目元数据
    using HashIndex = std::unordered_map<usize, std::vector<usize>>;
    HashIndex           hash_index_;    // hash → entry 下标列表

    usize next_chunk_idx_;  // 当前可写入的 chunk

    static constexpr usize DEFAULT_CHUNK_SIZE = 64 * 1024;  // 64KB

    void  alloc_chunk(usize min_capacity = DEFAULT_CHUNK_SIZE);
    u8*   alloc_in_chunk(usize size);
    usize compute_hash(const u8* data, usize byte_length) const noexcept;
};

}  // namespace ca::str
