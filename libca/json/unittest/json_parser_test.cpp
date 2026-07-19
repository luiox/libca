#include "libca/json/json.hpp"

#include <gtest/gtest.h>

#include <string>
#include <vector>

using namespace ca::json;
using ca::str::Utf8StringRef;
using ca::str::Utf8String;
using ca::str::Utf8StringArena;

namespace {

// 把事件记录成字符串序列，便于断言。
class RecordingHandler : public JsonHandler {
public:
    std::vector<std::string> events;
    std::string error;
    bool errored = false;

    void on_null(const SourceLocation&) override { events.push_back("null"); }
    void on_bool(bool v, const SourceLocation&) override {
        events.push_back(v ? "bool:true" : "bool:false");
    }
    void on_int(ca::i64 v, const SourceLocation&) override {
        events.push_back("int:" + std::to_string(v));
    }
    void on_float(ca::f64 v, const SourceLocation&) override {
        events.push_back("float:" + std::to_string(v));
    }
    void on_string(Utf8StringRef v, const SourceLocation&) override {
        events.push_back("str:" + to_std(v));
    }
    void on_array_start(const SourceLocation&) override { events.push_back("["); }
    void on_array_end(const SourceLocation&) override { events.push_back("]"); }
    void on_object_start(const SourceLocation&) override { events.push_back("{"); }
    void on_object_end(const SourceLocation&) override { events.push_back("}"); }
    void on_object_key(Utf8StringRef key, const SourceLocation&) override {
        events.push_back("key:" + to_std(key));
    }
    void on_error(const ParseError& err) override {
        errored = true;
        error = to_std(err.message);
    }

private:
    static std::string to_std(const Utf8StringRef& s) {
        return std::string(reinterpret_cast<const char*>(s.data()),
                           reinterpret_cast<const char*>(s.data()) + s.byte_length());
    }
};

Utf8StringRef R(const char* s) { return Utf8StringRef::from_cstr(s); }

// SAX 测试共用：自管 arena（与 JsonReader 路径下 arena 由 document 持有不同）。
struct SaxEnv {
    Utf8StringArena arena;
    RecordingHandler h;
};

}  // namespace

TEST(JsonParserSaxTest, EmitsEventsForScalar) {
    SaxEnv env;
    JsonParser p(R("42"), env.h, env.arena);
    EXPECT_TRUE(p.parse());
    ASSERT_EQ(env.h.events.size(), 1u);
    EXPECT_EQ(env.h.events[0], "int:42");
}

TEST(JsonParserSaxTest, EmitsEventsForArray) {
    SaxEnv env;
    JsonParser p(R("[1, \"x\", true]"), env.h, env.arena);
    EXPECT_TRUE(p.parse());
    const std::vector<std::string> expected = {
        "[", "int:1", "str:x", "bool:true", "]"
    };
    EXPECT_EQ(env.h.events, expected);
}

TEST(JsonParserSaxTest, EmitsEventsForObject) {
    SaxEnv env;
    JsonParser p(R("{\"a\": 1, \"b\": null}"), env.h, env.arena);
    EXPECT_TRUE(p.parse());
    const std::vector<std::string> expected = {
        "{", "key:a", "int:1", "key:b", "null", "}"
    };
    EXPECT_EQ(env.h.events, expected);
}

TEST(JsonParserSaxTest, EmitsEventsForNested) {
    SaxEnv env;
    JsonParser p(R("{\"arr\": [1, 2]}"), env.h, env.arena);
    EXPECT_TRUE(p.parse());
    const std::vector<std::string> expected = {
        "{", "key:arr", "[", "int:1", "int:2", "]", "}"
    };
    EXPECT_EQ(env.h.events, expected);
}

TEST(JsonParserSaxTest, ReportsErrorAndStops) {
    SaxEnv env;
    JsonParser p(R("[1, 2, ]"), env.h, env.arena);  // 尾随逗号，默认禁止
    EXPECT_FALSE(p.parse());
    EXPECT_TRUE(env.h.errored);
    EXPECT_FALSE(env.h.error.empty());
}

TEST(JsonParserSaxTest, ReportsErrorLocation) {
    SaxEnv env;
    // 第二行第三个字符非法（缺逗号）
    JsonParser p(R("[1\n 2]"), env.h, env.arena);
    EXPECT_FALSE(p.parse());
    EXPECT_TRUE(env.h.errored);
    // 错误位置应在第 2 行
    EXPECT_EQ(p.last_error().location.line, 2u);
}

TEST(JsonParserSaxTest, SaxDoesNotAllocateDom) {
    // 用 SAX 处理一个大数组，只统计元素个数，验证不强制构造 DOM
    SaxEnv env;
    // 1000 个元素的数组
    std::string big = "[";
    for (int i = 0; i < 1000; ++i) {
        if (i) big += ",";
        big += std::to_string(i);
    }
    big += "]";
    JsonParser p(Utf8StringRef::from_string_view(std::string_view(big)), env.h, env.arena);
    EXPECT_TRUE(p.parse());
    EXPECT_EQ(env.h.events.size(), 1002u);  // [ + 1000 个 int + ]
}
