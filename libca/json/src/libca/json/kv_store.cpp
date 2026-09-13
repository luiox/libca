#include "libca/json/kv_store.hpp"

#include "libca/fs/file_util.hpp"
#include "libca/json/json_reader.hpp"
#include "libca/json/json_writer.hpp"
#include "libca/str/utf8_string.hpp"

namespace ca::json {

namespace {

// 构造非语法类错误（读取失败 / 结构不符）：ParseError.location 无意义（取默认值），
// message 携带人读描述。message 含动态路径/键名，按原始字节保留（from_data_unchecked
// 不校验 UTF-8），避免校验失败抛异常。
ParseError make_kv_error(std::string message)
{
    ParseError error;
    error.message = ca::str::Utf8String::from_data_unchecked(
        reinterpret_cast<const ca::u8*>(message.data()), message.size());
    return error;
}

}  // namespace

KvStore::KvStore(std::string path) : path_(std::move(path))
{
    state_ = std::make_unique<State>();
}

KvStore::~KvStore() = default;

KvStore::KvStore(KvStore&& other) noexcept = default;
KvStore& KvStore::operator=(KvStore&& other) noexcept = default;

const KvStore::KvValue* KvStore::find_locked(const std::string& key) const
{
    auto it = state_->values.find(key);
    return it != state_->values.end() ? &it->second : nullptr;
}

// ==================== 加载 / 保存 ====================

Result<KvStore, ParseError> KvStore::load(const std::string& path)
{
    // BOM 自动剥离后交给解析器；文件不存在视为空存储（成功），其余读取失败走错误通道。
    auto read_result = ca::fs::FileUtil::read_all_text(path, true);
    if (read_result.is_err()) {
        const auto fs_error = read_result.unwrap_err();
        if (fs_error == ca::fs::FsError::FileNotFound) {
            return ca::Ok(KvStore(path));
        }
        std::string message = "读取存储文件失败: ";
        message += ca::fs::to_string(fs_error);
        message += ": ";
        message += path;
        return ca::Err(make_kv_error(std::move(message)));
    }
    std::string text = std::move(read_result).unwrap();

    auto doc_result = JsonReader::read(ca::str::Utf8StringRef::from_string_view(text));
    if (doc_result.is_err()) {
        // 解析错误原样透传（location + message 为解析器的准确信息）。
        return ca::Err(std::move(doc_result).unwrap_err());
    }
    auto document = std::move(doc_result).unwrap();

    const JsonValue& root = document.root();
    if (!root.is_object()) {
        return ca::Err(make_kv_error("存储根必须是 JSON 对象: " + path));
    }

    KvStore store(path);
    for (const auto& member : root.as_object()) {
        std::string key = member.first.to_std_string();
        const JsonValue& value = member.second;
        switch (value.type()) {
            case JsonType::Bool:
                store.state_->values.emplace(std::move(key), value.as_bool());
                break;
            case JsonType::Int:
                store.state_->values.emplace(std::move(key), value.as_int());
                break;
            case JsonType::Float:
                store.state_->values.emplace(std::move(key), value.as_float());
                break;
            case JsonType::String:
                store.state_->values.emplace(std::move(key),
                                             value.as_string().to_std_string());
                break;
            default: {
                // null/array/object 不是标量，拒绝加载以免下次 save 静默丢数据。
                std::string message = "键 \"" + key + "\" 的值类型不受支持（null/array/object）";
                return ca::Err(make_kv_error(std::move(message)));
            }
        }
    }
    // JSON 重复 key 时 emplace 首个生效，与 JsonValue::find()「返回首个」语义一致。
    return ca::Ok(std::move(store));
}

Result<void, ca::fs::FsError> KvStore::save() const
{
    if (!state_) {
        return ca::Err(ca::fs::FsError::Unknown);  // moved-from 对象不可保存
    }

    // 整程持锁（含文件 I/O）：并发 save 依次提交。若只锁快照、锁外写盘，
    // Windows 上两个 rename 同时替换同一目标会瞬时冲突（目标短暂 delete-pending）。
    std::lock_guard<std::mutex> lock(state_->mutex);
    const std::map<std::string, KvValue>& values = state_->values;

    JsonDocument document;
    document.root() = JsonValue::make_object();
    auto& arena = document.arena();
    for (const auto& entry : values) {
        JsonValue json_value;
        if (const auto* s = std::get_if<std::string>(&entry.second)) {
            json_value = JsonValue::make_string(arena.intern(*s));
        } else if (const auto* b = std::get_if<bool>(&entry.second)) {
            json_value = JsonValue::make_bool(*b);
        } else if (const auto* i = std::get_if<ca::i64>(&entry.second)) {
            json_value = JsonValue::make_int(*i);
        } else if (const auto* d = std::get_if<ca::f64>(&entry.second)) {
            json_value = JsonValue::make_float(*d);
        } else {
            return ca::Err(ca::fs::FsError::Unknown);  // variant 封闭，不可达
        }
        document.root().set(arena.intern(entry.first), std::move(json_value));
    }

    const auto text = JsonWriter::write(document);
    // 原子写回：fs::FileUtil 已有临时文件 + rename 提交入口，直接复用；
    // 失败时旧文件保持原样。
    return ca::fs::FileUtil::atomic_write_text(path_, text.to_std_string());
}

// ==================== 读取 ====================

Option<std::string> KvStore::get_string(const std::string& key) const
{
    if (!state_) {
        return None;  // moved-from 防御：按空存储读
    }
    std::lock_guard<std::mutex> lock(state_->mutex);
    const KvValue* value = find_locked(key);
    if (const auto* s = value ? std::get_if<std::string>(value) : nullptr) {
        return Some(*s);
    }
    return None;  // 不存在或类型不符视同缺失
}

std::string KvStore::get_string(const std::string& key, std::string default_value) const
{
    return get_string(key).unwrap_or(std::move(default_value));
}

Option<ca::i64> KvStore::get_int(const std::string& key) const
{
    if (!state_) {
        return None;
    }
    std::lock_guard<std::mutex> lock(state_->mutex);
    const KvValue* value = find_locked(key);
    if (const auto* i = value ? std::get_if<ca::i64>(value) : nullptr) {
        return Some(*i);
    }
    return None;
}

ca::i64 KvStore::get_int(const std::string& key, ca::i64 default_value) const
{
    return get_int(key).unwrap_or(default_value);
}

Option<bool> KvStore::get_bool(const std::string& key) const
{
    if (!state_) {
        return None;
    }
    std::lock_guard<std::mutex> lock(state_->mutex);
    const KvValue* value = find_locked(key);
    if (const auto* b = value ? std::get_if<bool>(value) : nullptr) {
        return Some(*b);
    }
    return None;
}

bool KvStore::get_bool(const std::string& key, bool default_value) const
{
    return get_bool(key).unwrap_or(default_value);
}

Option<ca::f64> KvStore::get_double(const std::string& key) const
{
    if (!state_) {
        return None;
    }
    std::lock_guard<std::mutex> lock(state_->mutex);
    const KvValue* value = find_locked(key);
    // Int 自动提升为 double；仅 Float / Int 视为命中。
    if (const auto* d = value ? std::get_if<ca::f64>(value) : nullptr) {
        return Some(*d);
    }
    if (const auto* i = value ? std::get_if<ca::i64>(value) : nullptr) {
        return Some(static_cast<ca::f64>(*i));
    }
    return None;
}

ca::f64 KvStore::get_double(const std::string& key, ca::f64 default_value) const
{
    return get_double(key).unwrap_or(default_value);
}

// ==================== 写入 ====================

void KvStore::set_string(const std::string& key, std::string value)
{
    if (!state_) {
        return;  // moved-from 防御：写为 no-op
    }
    std::lock_guard<std::mutex> lock(state_->mutex);
    state_->values[key] = std::move(value);
}

void KvStore::set_int(const std::string& key, ca::i64 value)
{
    if (!state_) {
        return;
    }
    std::lock_guard<std::mutex> lock(state_->mutex);
    state_->values[key] = value;
}

void KvStore::set_bool(const std::string& key, bool value)
{
    if (!state_) {
        return;
    }
    std::lock_guard<std::mutex> lock(state_->mutex);
    state_->values[key] = value;
}

void KvStore::set_double(const std::string& key, ca::f64 value)
{
    if (!state_) {
        return;
    }
    std::lock_guard<std::mutex> lock(state_->mutex);
    state_->values[key] = value;
}

// ==================== 结构操作 ====================

bool KvStore::remove(const std::string& key)
{
    if (!state_) {
        return false;
    }
    std::lock_guard<std::mutex> lock(state_->mutex);
    return state_->values.erase(key) > 0;
}

bool KvStore::contains(const std::string& key) const
{
    if (!state_) {
        return false;
    }
    std::lock_guard<std::mutex> lock(state_->mutex);
    return state_->values.find(key) != state_->values.end();
}

ca::usize KvStore::len() const
{
    if (!state_) {
        return 0;
    }
    std::lock_guard<std::mutex> lock(state_->mutex);
    return state_->values.size();
}

const std::string& KvStore::path() const noexcept
{
    return path_;
}

}  // namespace ca::json
