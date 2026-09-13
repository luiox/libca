/// @file kv_store.hpp
/// @brief 扁平 JSON 持久化键值存储：KvStore。
/// @author Canrad
/// @date 2026/09/13

#pragma once

#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <variant>

#include "libca/core/datatype.hpp"
#include "libca/core/option.hpp"
#include "libca/core/result.hpp"
#include "libca/fs/fs_error.hpp"
#include "libca/json/parse_error.hpp"

namespace ca::json {

// core 的 Option 族在本命名空间内引入 using 声明，避免非限定名经 ca 顶层
// 别名（ca::Option）与 ca::core::Option 查找二义（与 collection/stream.hpp 同法）。
using ca::core::None;
using ca::core::Option;
using ca::core::Some;

/// @brief 基于一层 JSON 对象的持久化键值存储。
///
/// 存储模型是「扁平一层」的 JSON 对象：key 为任意 UTF-8 字符串（原样存储，不做
/// 路径/嵌套切分，`"a.b"` 就是一个普通 key），value 仅支持 bool / int（i64）/
/// double（f64）/ string 四种标量。加载用 `KvStore::load(path)`，写回用
/// `save()`；落盘格式为紧凑 JSON（`JsonWriter` 输出），保存经
/// `fs::FileUtil::atomic_write_text` 临时文件 + rename 原子提交，失败不破坏旧文件。
///
/// 加载语义：
/// - 文件不存在 → 返回空存储（成功），首次保存时创建文件；
/// - JSON 非法 → Err（ParseError，携带解析位置与人读消息）；
/// - 根不是 JSON 对象、或成员值含 null/array/object → Err（ParseError，位置为默认值）；
/// - 文件带 BOM → 自动剥离（复用 str 模块 strip_bom）后再解析。
///
/// 类型纪律：get_int 只接受 Int；get_double 接受 Int 与 Float（数值提升）；
/// get_bool / get_string 严格同型。类型不符视同键不存在（返回默认值 / None）。
/// 保存时 double 经 `%.17g` 序列化，round-trip 精度无损；整数形态的 double
/// （如 2.0）写回补 `.0` 保持 Float 形态。
///
/// @note 线程安全：内部以一把 std::mutex 串行化全部状态访问，多线程并发调用安全；
///       save() 整程持锁（含文件写入），并发 save 依次提交，保存的是当次提交前的
///       完整状态。锁不可重入，但方法不会回调用户代码，无死锁风险。
/// @note 对象仅可移动（std::mutex 不可复制/移动，状态经 unique_ptr 间接持有）；
///       moved-from 对象除销毁与移动赋值外不可用（读按空存储、写为 no-op）。
/// @note 内部用 std::map（按 key 字节序），保存文件内容确定性强，便于 diff 与人工检查。
class KvStore {
public:
    /// @brief 构造绑定输出路径的空存储。@param path 保存目标文件路径（UTF-8）。
    explicit KvStore(std::string path);

    ~KvStore();

    KvStore(const KvStore&)            = delete;
    KvStore& operator=(const KvStore&) = delete;
    KvStore(KvStore&& other) noexcept;
    KvStore& operator=(KvStore&& other) noexcept;

    /// @brief 从 JSON 文件加载键值存储。
    /// @param path 存储文件路径（UTF-8）。
    /// @return 成功返回 KvStore（文件不存在时为空存储）；JSON 非法 / 根非对象 /
    ///         含不支持的值类型返回 ParseError；读取失败（权限等）同样以 ParseError
    ///         返回（location 为默认值，message 记录 fs 错误原因）。
    static Result<KvStore, ParseError> load(const std::string& path);

    /// @brief 原子写回绑定路径（构造或 load 时的 path）。
    /// @return 成功返回 Ok；写失败返回 FsError（旧文件保持原样）。
    Result<void, ca::fs::FsError> save() const;

    // ==================== 读取 ====================

    /// @brief 取字符串值。@return 键不存在或类型不符返回 None。
    Option<std::string> get_string(const std::string& key) const;
    /// @brief 取字符串值；键不存在或类型不符返回 default_value。
    std::string get_string(const std::string& key, std::string default_value) const;

    /// @brief 取整数值（仅 Int）。@return 键不存在或类型不符返回 None。
    Option<ca::i64> get_int(const std::string& key) const;
    /// @brief 取整数值（仅 Int）；键不存在或类型不符返回 default_value。
    ca::i64 get_int(const std::string& key, ca::i64 default_value) const;

    /// @brief 取布尔值。@return 键不存在或类型不符返回 None。
    Option<bool> get_bool(const std::string& key) const;
    /// @brief 取布尔值；键不存在或类型不符返回 default_value。
    bool get_bool(const std::string& key, bool default_value) const;

    /// @brief 取浮点值（Int 自动提升为 double）。@return 键不存在或类型不符返回 None。
    Option<ca::f64> get_double(const std::string& key) const;
    /// @brief 取浮点值（Int 自动提升为 double）；键不存在或类型不符返回 default_value。
    ca::f64 get_double(const std::string& key, ca::f64 default_value) const;

    // ==================== 写入 ====================

    /// @brief 设置字符串值，覆盖同名 key（含类型替换）。
    void set_string(const std::string& key, std::string value);
    /// @brief 设置整数值，覆盖同名 key（含类型替换）。
    void set_int(const std::string& key, ca::i64 value);
    /// @brief 设置布尔值，覆盖同名 key（含类型替换）。
    void set_bool(const std::string& key, bool value);
    /// @brief 设置浮点值，覆盖同名 key（含类型替换）。
    void set_double(const std::string& key, ca::f64 value);

    // ==================== 结构操作 ====================

    /// @brief 移除 key。@return 实际删除了条目返回 true。
    bool remove(const std::string& key);
    /// @brief 判断 key 是否存在。
    bool contains(const std::string& key) const;
    /// @brief 条目数量。
    ca::usize len() const;

    /// @brief 绑定的保存路径（构造或 load 时传入）。
    const std::string& path() const noexcept;

private:
    /// 标量值：bool / i64 / f64 / string 四选一。
    using KvValue = std::variant<bool, ca::i64, ca::f64, std::string>;

    /// 共享状态：std::mutex 不可移动，包一层让 KvStore 本体可移动。
    struct State {
        std::mutex mutex;
        std::map<std::string, KvValue> values;  ///< 按 key 字节序，序列化顺序确定
    };

    /// 按 key 查值；未找到或类型不符返回 nullptr。调用方须已持锁。
    const KvValue* find_locked(const std::string& key) const;

    std::string path_;                ///< 保存目标路径（构造后不可变）
    std::unique_ptr<State> state_;    ///< 互斥锁 + 键值表；经指针间接持有以支持移动
};

}  // namespace ca::json
