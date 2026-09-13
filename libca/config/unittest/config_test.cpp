#include <gtest/gtest.h>

#include "libca/config/config.hpp"
#include "libca/config/config_error.hpp"
#include "libca/config/config_var.hpp"
#include "libca/config/lexical_cast.hpp"

#include "libca/core/datatype.hpp"

#include "libca/json/json_document.hpp"
#include "libca/json/json_reader.hpp"
#include "libca/json/json_value.hpp"
#include "libca/str/utf8_string.hpp"

#include <memory>
#include <string>
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

}  // namespace
