#include <gtest/gtest.h>

#include "libca/config/config.hpp"
#include "libca/config/config_error.hpp"
#include "libca/config/config_var.hpp"
#include "libca/config/lexical_cast.hpp"

#include "libca/core/datatype.hpp"

#include "libca/fs/file_util.hpp"

#include "libca/json/json_document.hpp"
#include "libca/json/json_reader.hpp"
#include "libca/json/json_value.hpp"
#include "libca/str/utf8_string.hpp"

#include <atomic>
#include <chrono>
#include <memory>
#include <string>
#include <thread>
#include <typeindex>
#include <unordered_map>
#include <vector>

// config 模块单元测试。
// 注册中心是进程级单例：fixture 每个用例前 clear()，用例内 key 用独立前缀双保险。

namespace {

using ca::config::Config;
using ca::config::ConfigError;
using ca::config::ConfigVar;

class ConfigTest : public ::testing::Test {
protected:
    void SetUp() override
    {
        Config::clear();
    }
};

// ==================== lookup：创建 / 幂等 / 类型冲突 ====================

// 不存在的 name：创建并带默认值、名字与描述。
TEST_F(ConfigTest, LookupCreatesWithDefault)
{
    auto var = Config::lookup<ca::i32>("t1/server/port", 8080, "服务端口");
    ASSERT_NE(var, nullptr);
    EXPECT_EQ(var->value(), 8080);
    EXPECT_EQ(var->name(), "t1/server/port");
    EXPECT_EQ(var->description(), "服务端口");
    EXPECT_EQ(var->type(), std::type_index(typeid(ca::i32)));
}

// 同名同类型重复 lookup：返回同一实例，忽略第二次的 default 与 description（幂等）。
TEST_F(ConfigTest, LookupIdempotentReturnsSameInstance)
{
    auto first = Config::lookup<ca::i32>("t1/idempotent", 1, "first");
    auto second = Config::lookup<ca::i32>("t1/idempotent", 999, "second");
    ASSERT_NE(first, nullptr);
    ASSERT_NE(second, nullptr);
    EXPECT_EQ(first.get(), second.get());
    EXPECT_EQ(first->value(), 1);          // 第二次的 default 999 被忽略
    EXPECT_EQ(first->description(), "first");  // 第二次的描述也被忽略

    // 类型擦除视图也指向同一实例
    auto base = Config::lookup_base("t1/idempotent");
    ASSERT_NE(base, nullptr);
    EXPECT_EQ(base.get(), first.get());
}

// 同名不同类型：后一次 lookup 返回 nullptr，且不破坏已注册实例。
TEST_F(ConfigTest, LookupTypeConflictReturnsNullptr)
{
    auto int_var = Config::lookup<ca::i32>("t1/conflict", 7, "int");
    ASSERT_NE(int_var, nullptr);

    auto f64_var = Config::lookup<ca::f64>("t1/conflict", 1.0, "f64");
    EXPECT_EQ(f64_var, nullptr);

    auto str_var = Config::lookup<std::string>("t1/conflict", std::string("s"), "string");
    EXPECT_EQ(str_var, nullptr);

    // 冲突的 lookup 不影响原实例
    auto again = Config::lookup<ca::i32>("t1/conflict", 0);
    ASSERT_NE(again, nullptr);
    EXPECT_EQ(again.get(), int_var.get());
    EXPECT_EQ(again->value(), 7);
}

// lookup_base：存在的 name 返回实例；不存在的 name 返回 nullptr。
TEST_F(ConfigTest, LookupBaseMissingReturnsNullptr)
{
    EXPECT_EQ(Config::lookup_base("t1/missing"), nullptr);
}

// ==================== ConfigVar：set 短路与监听器 ====================

// set 值相等短路：不换值、不触发监听器、返回 false。
TEST_F(ConfigTest, SetEqualShortCircuits)
{
    ConfigVar<ca::i32> var("direct/var", 10);
    int change_count = 0;
    const ca::u64 id = var.add_listener(
        [&change_count](const ca::i32& old_value, const ca::i32& new_value) {
            (void)old_value;
            (void)new_value;
            ++change_count;
        });

    EXPECT_FALSE(var.set(10));
    EXPECT_EQ(change_count, 0);
    EXPECT_EQ(var.value(), 10);

    EXPECT_TRUE(var.set(11));
    EXPECT_EQ(change_count, 1);
    EXPECT_EQ(var.value(), 11);

    // remove_listener 后：换值仍生效，但不再回调
    EXPECT_TRUE(var.remove_listener(id));
    EXPECT_TRUE(var.set(12));
    EXPECT_EQ(change_count, 1);
    EXPECT_EQ(var.value(), 12);
    EXPECT_FALSE(var.remove_listener(id));  // 重复移除返回 false
    EXPECT_FALSE(var.remove_listener(0));   // 0 保留为无效 id
}

// 监听器收到正确的旧值/新值；多个监听器都触发；id 从 1 递增且互不相同。
TEST_F(ConfigTest, ListenerReceivesOldAndNewValue)
{
    ConfigVar<std::string> var("direct/listener", std::string("old"));
    ca::i32 observed_old_len = -1;
    ca::i32 observed_new_len = -1;
    int fire_count = 0;

    const ca::u64 id1 = var.add_listener(
        [&observed_old_len, &observed_new_len, &fire_count](const std::string& old_value,
                                                            const std::string& new_value) {
            observed_old_len = static_cast<ca::i32>(old_value.size());
            observed_new_len = static_cast<ca::i32>(new_value.size());
            ++fire_count;
        });
    const ca::u64 id2 = var.add_listener(
        [&fire_count](const std::string&, const std::string&) { ++fire_count; });

    EXPECT_NE(id1, ca::u64(0));
    EXPECT_NE(id2, ca::u64(0));
    EXPECT_NE(id1, id2);

    EXPECT_TRUE(var.set(std::string("brand-new")));
    EXPECT_EQ(fire_count, 2);
    EXPECT_EQ(observed_old_len, 3);       // "old"
    EXPECT_EQ(observed_new_len, 9);       // "brand-new"
    EXPECT_EQ(var.value(), "brand-new");
}

// ==================== JsonCast：标量 ====================

// bool：严格类型校验。
TEST_F(ConfigTest, JsonCastBool)
{
    auto ok = ca::config::JsonCast<bool>::from_json(ca::json::JsonValue::make_bool(true));
    ASSERT_TRUE(ok.is_ok());
    EXPECT_TRUE(std::move(ok).unwrap());

    auto bad = ca::config::JsonCast<bool>::from_json(ca::json::JsonValue::make_int(1));
    ASSERT_TRUE(bad.is_err());
    EXPECT_EQ(std::move(bad).unwrap_err(), ConfigError::TYPE_MISMATCH);
}

// 整型：范围检查（u32 越界报 OUT_OF_RANGE，i8 越界同理），负数对无符号越界。
TEST_F(ConfigTest, JsonCastIntegerRangeCheck)
{
    auto u32_ok = ca::config::JsonCast<ca::u32>::from_json(ca::json::JsonValue::make_int(42));
    ASSERT_TRUE(u32_ok.is_ok());
    EXPECT_EQ(std::move(u32_ok).unwrap(), ca::u32(42));

    auto u32_big = ca::config::JsonCast<ca::u32>::from_json(ca::json::JsonValue::make_int(5000000000));
    ASSERT_TRUE(u32_big.is_err());
    EXPECT_EQ(std::move(u32_big).unwrap_err(), ConfigError::OUT_OF_RANGE);

    auto u32_neg = ca::config::JsonCast<ca::u32>::from_json(ca::json::JsonValue::make_int(-1));
    ASSERT_TRUE(u32_neg.is_err());
    EXPECT_EQ(std::move(u32_neg).unwrap_err(), ConfigError::OUT_OF_RANGE);

    auto i64_ok = ca::config::JsonCast<ca::i64>::from_json(
        ca::json::JsonValue::make_int(9007199254740993));
    ASSERT_TRUE(i64_ok.is_ok());
    EXPECT_EQ(std::move(i64_ok).unwrap(), ca::i64(9007199254740993));

    // 整型不接受 JSON Float
    auto i32_float = ca::config::JsonCast<ca::i32>::from_json(ca::json::JsonValue::make_float(1.5));
    ASSERT_TRUE(i32_float.is_err());
    EXPECT_EQ(std::move(i32_float).unwrap_err(), ConfigError::TYPE_MISMATCH);
}

// 浮点：Int 与 Float 都接受。
TEST_F(ConfigTest, JsonCastFloatAcceptsIntAndFloat)
{
    auto from_int = ca::config::JsonCast<ca::f64>::from_json(ca::json::JsonValue::make_int(3));
    ASSERT_TRUE(from_int.is_ok());
    EXPECT_DOUBLE_EQ(std::move(from_int).unwrap(), 3.0);

    auto from_float = ca::config::JsonCast<ca::f32>::from_json(ca::json::JsonValue::make_float(2.5));
    ASSERT_TRUE(from_float.is_ok());
    EXPECT_FLOAT_EQ(std::move(from_float).unwrap(), 2.5f);

    auto bad = ca::config::JsonCast<ca::f64>::from_json(ca::json::JsonValue::make_string(
        ca::str::Utf8StringRef::from_cstr("3.14")));
    ASSERT_TRUE(bad.is_err());
    EXPECT_EQ(std::move(bad).unwrap_err(), ConfigError::TYPE_MISMATCH);
}

// 字符串：JSON String ↔ std::string。
TEST_F(ConfigTest, JsonCastString)
{
    ca::json::JsonDocument document;
    ca::json::JsonValue value = ca::json::JsonValue::make_string(document.arena().intern("hello"));
    auto ok = ca::config::JsonCast<std::string>::from_json(value);
    ASSERT_TRUE(ok.is_ok());
    EXPECT_EQ(std::move(ok).unwrap(), "hello");

    auto bad = ca::config::JsonCast<std::string>::from_json(ca::json::JsonValue::make_int(0));
    ASSERT_TRUE(bad.is_err());
    EXPECT_EQ(std::move(bad).unwrap_err(), ConfigError::TYPE_MISMATCH);
}

// ==================== JsonCast：嵌套容器 ====================

// vector<i32> / vector<string> 往返。
TEST_F(ConfigTest, JsonCastVectorContainers)
{
    {
        auto doc_result = ca::json::JsonReader::read(ca::str::Utf8StringRef::from_string_view("[1,2,3]"));
        ASSERT_TRUE(doc_result.is_ok());
        ca::json::JsonDocument document = std::move(doc_result).unwrap();

        auto converted = ca::config::JsonCast<std::vector<ca::i32>>::from_json(document.root());
        ASSERT_TRUE(converted.is_ok());
        EXPECT_EQ(std::move(converted).unwrap(), (std::vector<ca::i32>{1, 2, 3}));
    }
    {
        auto doc_result =
            ca::json::JsonReader::read(ca::str::Utf8StringRef::from_string_view("[\"a\",\"b\"]"));
        ASSERT_TRUE(doc_result.is_ok());
        ca::json::JsonDocument document = std::move(doc_result).unwrap();

        auto converted = ca::config::JsonCast<std::vector<std::string>>::from_json(document.root());
        ASSERT_TRUE(converted.is_ok());
        EXPECT_EQ(std::move(converted).unwrap(), (std::vector<std::string>{"a", "b"}));
    }
    // 元素类型不符：整体失败
    {
        auto doc_result =
            ca::json::JsonReader::read(ca::str::Utf8StringRef::from_string_view("[1,\"x\"]"));
        ASSERT_TRUE(doc_result.is_ok());
        ca::json::JsonDocument document = std::move(doc_result).unwrap();

        auto converted = ca::config::JsonCast<std::vector<ca::i32>>::from_json(document.root());
        ASSERT_TRUE(converted.is_err());
        EXPECT_EQ(std::move(converted).unwrap_err(), ConfigError::TYPE_MISMATCH);
    }
}

// map<string,i32> 往返。
TEST_F(ConfigTest, JsonCastMapContainer)
{
    auto doc_result =
        ca::json::JsonReader::read(ca::str::Utf8StringRef::from_string_view("{\"a\":1,\"b\":2}"));
    ASSERT_TRUE(doc_result.is_ok());
    ca::json::JsonDocument document = std::move(doc_result).unwrap();

    auto converted =
        ca::config::JsonCast<std::unordered_map<std::string, ca::i32>>::from_json(document.root());
    ASSERT_TRUE(converted.is_ok());
    const auto map = std::move(converted).unwrap();
    ASSERT_EQ(map.size(), ca::usize(2));
    EXPECT_EQ(map.at("a"), 1);
    EXPECT_EQ(map.at("b"), 2);
}

// map<string,vector<i32>> 嵌套往返（含序列化 → 再解析）。
TEST_F(ConfigTest, JsonCastNestedMapOfVectorRoundTrip)
{
    std::unordered_map<std::string, std::vector<ca::i32>> source;
    source["a"] = std::vector<ca::i32>{1, 2};
    source["b"] = std::vector<ca::i32>{3};

    ca::json::JsonDocument document;
    ca::json::JsonValue encoded =
        ca::config::JsonCast<std::unordered_map<std::string, std::vector<ca::i32>>>::to_json(
            source, document.arena());
    document.root() = encoded;

    auto converted =
        ca::config::JsonCast<std::unordered_map<std::string, std::vector<ca::i32>>>::from_json(
            document.root());
    ASSERT_TRUE(converted.is_ok());
    EXPECT_EQ(std::move(converted).unwrap(), source);
}

// to_json()（scratch arena 版本）：字符串内容正确。
TEST_F(ConfigTest, ConfigVarToJsonScratchArena)
{
    ConfigVar<std::vector<std::string>> var("direct/vec_str",
                                            std::vector<std::string>{"x", "y"});
    ca::json::JsonValue value = var.to_json();
    ASSERT_TRUE(value.is_array());
    ASSERT_EQ(value.size(), ca::usize(2));
    EXPECT_EQ(value.at(0).as_string().to_std_string(), "x");
    EXPECT_EQ(value.at(1).as_string().to_std_string(), "y");
}

// ==================== load：注册项应用 / 监听器 ====================

// load 应用到已注册 var：监听器触发一次，旧值/新值正确。
TEST_F(ConfigTest, LoadAppliesRegisteredVarAndFiresListener)
{
    auto var = Config::lookup<ca::i32>("t2/port", 80);
    ca::i32 observed_old = 0;
    ca::i32 observed_new = 0;
    int fire_count = 0;
    var->add_listener([&observed_old, &observed_new, &fire_count](const ca::i32& old_value,
                                                                  const ca::i32& new_value) {
        observed_old = old_value;
        observed_new = new_value;
        ++fire_count;
    });

    const auto result = Config::load(R"({"t2/port": 8080})");
    ASSERT_TRUE(result.is_ok());
    EXPECT_EQ(var->value(), 8080);
    ASSERT_EQ(fire_count, 1);
    EXPECT_EQ(observed_old, 80);
    EXPECT_EQ(observed_new, 8080);
}

// load 的值与当前值相等：不触发监听器。
TEST_F(ConfigTest, LoadEqualValueDoesNotFireListener)
{
    auto var = Config::lookup<ca::i32>("t2/same", 42);
    int fire_count = 0;
    var->add_listener([&fire_count](const ca::i32&, const ca::i32&) { ++fire_count; });

    const auto result = Config::load(R"({"t2/same": 42})");
    ASSERT_TRUE(result.is_ok());
    EXPECT_EQ(fire_count, 0);
}

// load 未知 key：存入未物化表，后续 lookup 物化时以已加载值为初值。
TEST_F(ConfigTest, LoadUnknownKeyMaterializedOnLookup)
{
    const auto result = Config::load(R"({"t2/future": 7})");
    ASSERT_TRUE(result.is_ok());

    auto var = Config::lookup<ca::i32>("t2/future", 0, "后注册");
    ASSERT_NE(var, nullptr);
    EXPECT_EQ(var->value(), 7);

    // 物化后未物化表为空：visit 只剩已注册条目
    std::vector<ca::config::ConfigEntry> entries;
    Config::visit([&entries](const ca::config::ConfigEntry& entry) { entries.push_back(entry); });
    ASSERT_EQ(entries.size(), ca::usize(1));
    EXPECT_FALSE(entries[0].pending);
    EXPECT_EQ(entries[0].value, "7");
}

// 未物化 key 与 lookup 类型不符：退回本次 default（文档化语义），注册仍成功。
TEST_F(ConfigTest, LoadUnknownKeyFallsBackToDefaultOnMismatch)
{
    const auto result = Config::load(R"({"t2/fallback": "not-a-number"})");
    ASSERT_TRUE(result.is_ok());

    auto var = Config::lookup<ca::i32>("t2/fallback", 5);
    ASSERT_NE(var, nullptr);
    EXPECT_EQ(var->value(), 5);
}

// 嵌套容器经 load 注入：vector<int> / vector<string> / map<string,int> / map<string,vector<int>>。
TEST_F(ConfigTest, LoadIntoNestedContainerVars)
{
    auto vec_int = Config::lookup<std::vector<ca::i32>>("t2/vec_int", {});
    auto vec_str = Config::lookup<std::vector<std::string>>("t2/vec_str", {});
    auto map_int = Config::lookup<std::unordered_map<std::string, ca::i32>>("t2/map_int", {});
    auto map_vec =
        Config::lookup<std::unordered_map<std::string, std::vector<ca::i32>>>("t2/map_vec", {});

    const auto result = Config::load(R"({
        "t2/vec_int": [1, 2, 3],
        "t2/vec_str": ["a", "b"],
        "t2/map_int": {"x": 10, "y": 20},
        "t2/map_vec": {"k": [7, 8], "m": [9]}
    })");
    ASSERT_TRUE(result.is_ok());

    EXPECT_EQ(vec_int->value(), (std::vector<ca::i32>{1, 2, 3}));
    EXPECT_EQ(vec_str->value(), (std::vector<std::string>{"a", "b"}));

    const auto map_int_value = map_int->value();
    ASSERT_EQ(map_int_value.size(), ca::usize(2));
    EXPECT_EQ(map_int_value.at("x"), 10);
    EXPECT_EQ(map_int_value.at("y"), 20);

    const auto map_vec_value = map_vec->value();
    EXPECT_EQ(map_vec_value.at("k"), (std::vector<ca::i32>{7, 8}));
    EXPECT_EQ(map_vec_value.at("m"), (std::vector<ca::i32>{9}));
}

// ==================== load：失败语义 ====================

// 非法 JSON / 顶层非 object：整体拒绝，内部状态零变化。
TEST_F(ConfigTest, LoadInvalidJsonRejectedAtomically)
{
    auto var = Config::lookup<ca::i32>("t2/atomic", 1);
    int fire_count = 0;
    var->add_listener([&fire_count](const ca::i32&, const ca::i32&) { ++fire_count; });

    auto bad_syntax = Config::load(R"({"t2/atomic": )");
    ASSERT_TRUE(bad_syntax.is_err());
    EXPECT_EQ(std::move(bad_syntax).unwrap_err().code, ConfigError::PARSE_FAILED);

    auto not_object = Config::load("[1, 2, 3]");
    ASSERT_TRUE(not_object.is_err());
    EXPECT_EQ(std::move(not_object).unwrap_err().code, ConfigError::ROOT_NOT_OBJECT);

    // 两次整体失败：值与监听器均未动，注册表也无新增条目
    EXPECT_EQ(var->value(), 1);
    EXPECT_EQ(fire_count, 0);
    std::vector<ca::config::ConfigEntry> entries;
    Config::visit([&entries](const ca::config::ConfigEntry& entry) { entries.push_back(entry); });
    ASSERT_EQ(entries.size(), ca::usize(1));
    EXPECT_FALSE(entries[0].pending);
}

// 单 key 类型不匹配：跳过该 key、其余 key 生效，Err 携带失败 key 详情。
TEST_F(ConfigTest, LoadSingleKeyMismatchSkipsAndReports)
{
    auto good = Config::lookup<ca::i32>("t2/good", 0);
    auto bad = Config::lookup<ca::i32>("t2/bad", 0);
    int bad_fire = 0;
    bad->add_listener([&bad_fire](const ca::i32&, const ca::i32&) { ++bad_fire; });

    const auto result = Config::load(R"({"t2/good": 3, "t2/bad": "text"})");
    ASSERT_TRUE(result.is_err());
    const ca::config::ConfigErrorInfo info = std::move(result).unwrap_err();
    EXPECT_EQ(info.code, ConfigError::TYPE_MISMATCH);
    ASSERT_EQ(info.keys.size(), ca::usize(1));
    EXPECT_EQ(info.keys[0], "t2/bad");
    EXPECT_NE(info.message.find("t2/bad"), std::string::npos);

    // 好的 key 生效；坏的 key 保持原值且未触发监听器
    EXPECT_EQ(good->value(), 3);
    EXPECT_EQ(bad->value(), 0);
    EXPECT_EQ(bad_fire, 0);
}

// ==================== load_file / visit ====================

// load_file 往返：写临时文件 → load_file → 断言 → 删除。
TEST_F(ConfigTest, LoadFileRoundTrip)
{
    const auto temp_result = ca::fs::FileUtil::create_temp_file("libca_config_test", ".json");
    ASSERT_TRUE(temp_result.is_ok());
    const std::string path = std::move(temp_result).unwrap();

    const auto write_result = ca::fs::FileUtil::write_text(path, R"({"t2/file_var": 123})");
    ASSERT_TRUE(write_result.is_ok());

    auto var = Config::lookup<ca::i32>("t2/file_var", 0);
    const auto load_result = Config::load_file(path);
    ASSERT_TRUE(load_result.is_ok());
    EXPECT_EQ(var->value(), 123);

    EXPECT_TRUE(ca::fs::FileUtil::remove(path));
}

// load_file：文件不存在返回 READ_FILE_FAILED。
TEST_F(ConfigTest, LoadFileMissingReportsReadFileFailed)
{
    const auto result = Config::load_file("t2/no/such/file.json");
    ASSERT_TRUE(result.is_err());
    EXPECT_EQ(std::move(result).unwrap_err().code, ConfigError::READ_FILE_FAILED);
}

// visit：同时覆盖已注册 var 与未物化条目，name + JSON 文本表示正确。
TEST_F(ConfigTest, VisitCoversRegisteredAndPending)
{
    auto var = Config::lookup<ca::i32>("t2/registered", 9);
    (void)var;
    const auto result = Config::load(R"({"t2/pending_key": true})");
    ASSERT_TRUE(result.is_ok());

    std::vector<ca::config::ConfigEntry> entries;
    Config::visit([&entries](const ca::config::ConfigEntry& entry) { entries.push_back(entry); });
    ASSERT_EQ(entries.size(), ca::usize(2));

    for (const auto& entry : entries) {
        if (entry.name == "t2/registered") {
            EXPECT_FALSE(entry.pending);
            EXPECT_EQ(entry.value, "9");
        } else if (entry.name == "t2/pending_key") {
            EXPECT_TRUE(entry.pending);
            EXPECT_EQ(entry.value, "true");
        } else {
            FAIL() << "unexpected entry: " << entry.name;
        }
    }
}

// ==================== 并发 smoke ====================

// 2 线程 lookup + 1 线程 load + 原子计数监听器：总截止 5 秒内完成，禁止无界等待。
// 各线程循环自检 deadline，join 必然有界；并发加固（注册表 shared_mutex、监听器锁外
// 回调）已随提交 1/2 的实现自然涵盖，本用例做行为级回归。
TEST_F(ConfigTest, ConcurrentLookupLoadSmoke)
{
    namespace chrono = std::chrono;
    const auto deadline = chrono::steady_clock::now() + chrono::seconds(5);

    std::atomic<ca::i32> change_count{0};
    std::atomic<bool> stop{false};

    // 热点 key：lookup / load / 监听器三方并发
    auto hot = Config::lookup<ca::i32>("t3/hot", 0, "并发热点");
    ASSERT_NE(hot, nullptr);
    const ca::u64 listener_id = hot->add_listener([&change_count](const ca::i32&, const ca::i32&) {
        change_count.fetch_add(1, std::memory_order_relaxed);
    });

    auto lookup_hot_worker = [&stop, &deadline]() {
        ca::i32 turns = 0;
        while (!stop.load(std::memory_order_relaxed) && chrono::steady_clock::now() < deadline
               && turns < 500) {
            const auto var = Config::lookup<ca::i32>("t3/hot", turns);
            EXPECT_NE(var, nullptr);  // 幂等命中，绝不类型冲突
            ++turns;
        }
    };

    auto lookup_fresh_worker = [&stop, &deadline]() {
        ca::i32 turns = 0;
        while (!stop.load(std::memory_order_relaxed) && chrono::steady_clock::now() < deadline
               && turns < 500) {
            const auto var = Config::lookup<ca::i32>("t3/fresh/" + std::to_string(turns), turns);
            EXPECT_NE(var, nullptr);  // 新 key 逐个注册
            ++turns;
        }
    };

    auto load_worker = [&stop, &deadline]() {
        ca::i32 turns = 0;
        while (!stop.load(std::memory_order_relaxed) && chrono::steady_clock::now() < deadline
               && turns < 500) {
            const std::string text = "{\"t3/hot\": " + std::to_string(turns % 4) + "}";
            const auto result = Config::load(text);
            EXPECT_TRUE(result.is_ok());  // i32 var + 小整数，不应失败
            ++turns;
        }
    };

    std::thread t1(lookup_hot_worker);
    std::thread t2(lookup_fresh_worker);
    std::thread t3(load_worker);
    t1.join();
    t2.join();
    t3.join();
    stop.store(true);  // 线程均已在 deadline 内自行退出，此处仅防御

    // join 在截止时间内完成（留 1 秒调度余量）
    EXPECT_LT(chrono::steady_clock::now(), deadline + chrono::seconds(1));
    // 热点值始终是 load 序列的合法值（0..3，创建默认也是 0）
    const ca::i32 final_value = hot->value();
    EXPECT_GE(final_value, 0);
    EXPECT_LE(final_value, 3);
    // load 交替写 0..3，至少发生一次真实变化；计数不超过 load 轮次
    EXPECT_GT(change_count.load(), 0);
    EXPECT_TRUE(hot->remove_listener(listener_id));
}

}  // namespace
