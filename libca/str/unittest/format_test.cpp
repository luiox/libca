#include <gtest/gtest.h>

#include <string>
#include <stdexcept>

#include "libca/str/format.hpp"

using namespace ca::str;

// ============================================================
// format —— 编译期校验，返回 Utf8String
// ============================================================

TEST(FormatTest, basic_int_string) {
    auto s = format("port={}", 8080);
    EXPECT_EQ(s.byte_length(), 9u);
    EXPECT_STREQ(s.c_str(), "port=8080");
}

TEST(FormatTest, multiple_args) {
    auto s = format("{}, {}!", "hello", "world");
    EXPECT_STREQ(s.c_str(), "hello, world!");
}

TEST(FormatTest, empty_format_string) {
    auto s = format("");
    EXPECT_TRUE(s.is_empty());
    EXPECT_EQ(s.byte_length(), 0u);
}

TEST(FormatTest, no_args_literal) {
    auto s = format("just a literal");
    EXPECT_STREQ(s.c_str(), "just a literal");
}

TEST(FormatTest, returns_utf8string_with_correct_semantics) {
    auto s = format("x={}", 42);
    // Utf8String 语义：c_str / length / byte_length
    EXPECT_STREQ(s.c_str(), "x=42");
    EXPECT_EQ(s.length(), 4u);
    EXPECT_EQ(s.byte_length(), 4u);
    // move 语义：通过 move 构造新对象
    Utf8String moved = std::move(s);
    EXPECT_STREQ(moved.c_str(), "x=42");
}

TEST(FormatTest, various_types) {
    auto s = format("int={},uint={},float={:.2f},char={}", -5, 10u, 3.14159, 'A');
    EXPECT_STREQ(s.c_str(), "int=-5,uint=10,float=3.14,char=A");
}

// ============================================================
// UTF-8 内容
// ============================================================

TEST(FormatTest, utf8_content) {
    // "中文" = 6 字节，2 码点
    auto s = format("msg={}", std::string("\xe4\xb8\xad\xe6\x96\x87"));
    EXPECT_STREQ(s.c_str(), "msg=\xe4\xb8\xad\xe6\x96\x87");
    EXPECT_EQ(s.byte_length(), 10u);  // "msg=" 4 字节 + 中文 6 字节
    EXPECT_EQ(s.length(), 6u);         // m/s/g/=/中/文 共 6 码点
}

TEST(FormatTest, utf8_in_format_string) {
    // 格式串本身含 UTF-8 字面量
    auto s = format("值={}", 100);
    EXPECT_EQ(s.byte_length(), 7u);  // "值=" 4 字节 + "100" 3 字节
    EXPECT_EQ(s.length(), 5u);        // 值/=/1/0/0 共 5 码点
}

// ============================================================
// Utf8String / Utf8StringRef 作为 fmt 参数
// （验证是否需要 formatter 特化；fmt 对有 operator string_view() 的类型
//   不一定自动识别，若此用例编译失败需补 fmt::formatter 特化）
// ============================================================

TEST(FormatTest, utf8string_as_arg) {
    Utf8String name = Utf8String::from_cstr("libca");
    auto s = format("name={}", name);
    EXPECT_STREQ(s.c_str(), "name=libca");
}

TEST(FormatTest, utf8stringref_as_arg) {
    Utf8String name = Utf8String::from_cstr("libca");
    auto ref = name.ref();
    auto s = format("ref={}", ref);
    EXPECT_STREQ(s.c_str(), "ref=libca");
}

// ============================================================
// format_to(Utf8StringBuilder)
// ============================================================

TEST(FormatTest, format_to_builder_single) {
    Utf8StringBuilder b;
    format_to(b, "a={}, ", 1);
    format_to(b, "b={}", 2);
    auto s = b.build();
    EXPECT_STREQ(s.c_str(), "a=1, b=2");
}

TEST(FormatTest, format_to_builder_preserves_content) {
    Utf8StringBuilder b;
    b.append("prefix|");
    format_to(b, "val={}", 99);
    auto s = b.build();
    EXPECT_STREQ(s.c_str(), "prefix|val=99");
}

TEST(FormatTest, format_to_builder_returns_ref) {
    Utf8StringBuilder b;
    auto& ref = format_to(b, "x={}", 1);
    EXPECT_EQ(&ref, &b);  // 返回 out 的引用，支持链式
}

TEST(FormatTest, format_to_builder_utf8_then_build_validates) {
    Utf8StringBuilder b;
    format_to(b, "ok={}", 1);
    // 正常 UTF-8 build 成功
    auto s = b.build();
    EXPECT_STREQ(s.c_str(), "ok=1");
}

// ============================================================
// format_to(std::string) —— 不校验 UTF-8
// ============================================================

TEST(FormatTest, format_to_std_string) {
    std::string out;
    format_to(out, "x={}, ", 1);
    format_to(out, "y={}", 2);
    EXPECT_EQ(out, "x=1, y=2");
}

TEST(FormatTest, format_to_std_string_preserves_content) {
    std::string out = "pre|";
    format_to(out, "val={}", 42);
    EXPECT_EQ(out, "pre|val=42");
}

TEST(FormatTest, format_to_std_string_returns_ref) {
    std::string out;
    auto& ref = format_to(out, "{}", 1);
    EXPECT_EQ(&ref, &out);
}

// ============================================================
// format_runtime —— 运行期格式串
// ============================================================

TEST(FormatRuntimeTest, runtime_format_string) {
    // 模拟从配置读取的格式串（编译期无法校验）
    std::string fmt_str = std::string("port={}");
    int port = 8080;
    auto s = format_runtime(fmt_str, fmt::make_format_args(port));
    EXPECT_STREQ(s.c_str(), "port=8080");
}

TEST(FormatRuntimeTest, multiple_args) {
    std::string fmt_str = std::string("{}, {}!");
    std::string a = "hello";
    std::string b = "world";
    auto s = format_runtime(fmt_str, fmt::make_format_args(a, b));
    EXPECT_STREQ(s.c_str(), "hello, world!");
}

TEST(FormatRuntimeTest, returns_utf8string) {
    std::string fmt_str = std::string("x={}");
    int v = 42;
    auto s = format_runtime(fmt_str, fmt::make_format_args(v));
    EXPECT_EQ(s.byte_length(), 4u);
    EXPECT_EQ(s.length(), 4u);
}

// ============================================================
// 异常路径
// ============================================================

TEST(FormatTest, invalid_utf8_in_arg_throws) {
    // 构造一个会产出非法 UTF-8 字节的参数：直接传非法字节串
    // 非法序列 0xFF 0xFE 不是合法 UTF-8
    EXPECT_THROW(format("{}", std::string("\xff\xfe")), std::runtime_error);
}

TEST(FormatRuntimeTest, invalid_utf8_throws) {
    std::string fmt_str = std::string("{}");
    std::string bad = std::string("\xff\xfe");
    EXPECT_THROW(format_runtime(fmt_str, fmt::make_format_args(bad)), std::runtime_error);
}
