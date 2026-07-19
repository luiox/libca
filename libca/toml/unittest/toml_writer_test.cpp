#include "libca/toml/toml.hpp"

#include <gtest/gtest.h>

#include <utility>

using namespace ca;
using namespace ca::toml;
using ca::str::Utf8StringRef;
using ca::str::Utf8String;

namespace {

Utf8StringRef R(const char* s) { return Utf8StringRef::from_cstr(s); }

Utf8String write_str(const TomlDocument& doc, TomlWriterOptions opts = {}) {
    return TomlWriter::write(doc, opts);
}

Utf8String write_and_reparse(const TomlDocument& doc, TomlWriterOptions opts = {}) {
    Utf8String out = TomlWriter::write(doc, opts);
    auto r = TomlReader::read(out.ref());
    EXPECT_TRUE(r.is_ok()) << "writer output failed to reparse: " << out.c_str();
    if (r.is_err()) return Utf8String::from_cstr("");
    return TomlWriter::write(std::move(r).unwrap(), opts);
}

}  // namespace

// ============================================================================
// 标量序列化
// ============================================================================

TEST(TomlWriterTest, WritesSimpleKv) {
    TomlDocument doc;
    doc.root().set(R("name"), TomlValue::make_string(R("libca")));
    doc.root().set(R("version"), TomlValue::make_integer(1));
    Utf8String out = write_str(doc);
    EXPECT_NE(out.index_of(R("name")), ca::usize(-1));
    EXPECT_NE(out.index_of(R("libca")), ca::usize(-1));
    EXPECT_NE(out.index_of(R("version")), ca::usize(-1));
}

TEST(TomlWriterTest, WritesBoolean) {
    TomlDocument doc;
    doc.root().set(R("a"), TomlValue::make_boolean(true));
    doc.root().set(R("b"), TomlValue::make_boolean(false));
    Utf8String out = write_str(doc);
    EXPECT_NE(out.index_of(R("true")), ca::usize(-1));
    EXPECT_NE(out.index_of(R("false")), ca::usize(-1));
}

TEST(TomlWriterTest, WritesFloat) {
    TomlDocument doc;
    doc.root().set(R("pi"), TomlValue::make_float(3.14));
    Utf8String out = write_str(doc);
    // 浮点输出必须含 '.' 或 'e'，否则 reparse 会当整数
    EXPECT_NE(out.index_of(R(".")), ca::usize(-1));
}

TEST(TomlWriterTest, WritesFloatIntegerLikeGetsDotZero) {
    TomlDocument doc;
    doc.root().set(R("x"), TomlValue::make_float(1.0));
    Utf8String out = write_str(doc);
    // 1.0 应输出为 "1.0" 而非 "1"，否则 reparse 会变成整数
    EXPECT_NE(out.index_of(R("1.0")), ca::usize(-1));
}

TEST(TomlWriterTest, WritesInfNan) {
    TomlDocument doc;
    doc.root().set(R("inf"), TomlValue::make_float(std::numeric_limits<double>::infinity()));
    doc.root().set(R("ninf"), TomlValue::make_float(-std::numeric_limits<double>::infinity()));
    doc.root().set(R("nan"), TomlValue::make_float(std::numeric_limits<double>::quiet_NaN()));
    Utf8String out = write_str(doc);
    EXPECT_NE(out.index_of(R("inf")), ca::usize(-1));
    EXPECT_NE(out.index_of(R("-inf")), ca::usize(-1));
    EXPECT_NE(out.index_of(R("nan")), ca::usize(-1));
}

// ============================================================================
// Table / Array of Tables
// ============================================================================

TEST(TomlWriterTest, WritesNestedTable) {
    TomlDocument doc;
    TomlValue server = TomlValue::make_table();
    server.set(R("host"), TomlValue::make_string(R("localhost")));
    server.set(R("port"), TomlValue::make_integer(8080));
    doc.root().set(R("server"), std::move(server));
    Utf8String out = write_str(doc);
    EXPECT_NE(out.index_of(R("[server]")), ca::usize(-1));
}

TEST(TomlWriterTest, WritesArrayOfTables) {
    TomlDocument doc;
    TomlValue arr = TomlValue::make_array();
    TomlValue t1 = TomlValue::make_table();
    t1.set(R("name"), TomlValue::make_string(R("A")));
    TomlValue t2 = TomlValue::make_table();
    t2.set(R("name"), TomlValue::make_string(R("B")));
    arr.append(std::move(t1));
    arr.append(std::move(t2));
    doc.root().set(R("products"), std::move(arr));
    Utf8String out = write_str(doc);
    EXPECT_NE(out.index_of(R("[[products]]")), ca::usize(-1));
}

TEST(TomlWriterTest, WritesStringWithNewlinesMultiline) {
    TomlDocument doc;
    doc.root().set(R("s"), TomlValue::make_string(R("line1\nline2")));
    Utf8String out = write_str(doc);
    EXPECT_NE(out.index_of(R("\"\"\"")), ca::usize(-1));
}

// ============================================================================
// Round-trip：write → read → 应能拿到等价结构
// ============================================================================

TEST(TomlWriterTest, RoundTripScalars) {
    TomlDocument doc;
    doc.root().set(R("i"), TomlValue::make_integer(42));
    doc.root().set(R("f"), TomlValue::make_float(3.14));
    doc.root().set(R("b"), TomlValue::make_boolean(true));
    doc.root().set(R("s"), TomlValue::make_string(R("hello")));

    Utf8String out = write_str(doc);
    auto r = TomlReader::read(out.ref());
    ASSERT_TRUE(r.is_ok());
    TomlDocument back = std::move(r).unwrap();
    EXPECT_EQ(back.root().find(R("i"))->as_integer(), 42);
    EXPECT_DOUBLE_EQ(back.root().find(R("f"))->as_float(), 3.14);
    EXPECT_TRUE(back.root().find(R("b"))->as_boolean());
    EXPECT_EQ(back.root().find(R("s"))->as_string(), R("hello"));
}

TEST(TomlWriterTest, RoundTripTable) {
    TomlDocument doc;
    TomlValue server = TomlValue::make_table();
    server.set(R("host"), TomlValue::make_string(R("localhost")));
    server.set(R("port"), TomlValue::make_integer(8080));
    doc.root().set(R("server"), std::move(server));

    Utf8String out = write_str(doc);
    auto r = TomlReader::read(out.ref());
    ASSERT_TRUE(r.is_ok());
    TomlDocument back = std::move(r).unwrap();
    auto* s = back.root().find(R("server"));
    ASSERT_NE(s, nullptr);
    EXPECT_EQ(s->find(R("host"))->as_string(), R("localhost"));
    EXPECT_EQ(s->find(R("port"))->as_integer(), 8080);
}

TEST(TomlWriterTest, RoundTripArrayOfTables) {
    TomlDocument doc;
    TomlValue arr = TomlValue::make_array();
    TomlValue a = TomlValue::make_table();
    a.set(R("name"), TomlValue::make_string(R("A")));
    TomlValue b = TomlValue::make_table();
    b.set(R("name"), TomlValue::make_string(R("B")));
    arr.append(std::move(a));
    arr.append(std::move(b));
    doc.root().set(R("products"), std::move(arr));

    Utf8String out = write_str(doc);
    auto r = TomlReader::read(out.ref());
    ASSERT_TRUE(r.is_ok());
    TomlDocument back = std::move(r).unwrap();
    auto* p = back.root().find(R("products"));
    ASSERT_NE(p, nullptr);
    ASSERT_TRUE(p->is_array());
    EXPECT_EQ(p->size(), 2u);
    EXPECT_EQ(p->at(0).find(R("name"))->as_string(), R("A"));
    EXPECT_EQ(p->at(1).find(R("name"))->as_string(), R("B"));
}

TEST(TomlWriterTest, RoundTripDatetime) {
    TomlDocument doc;
    TomlDatetime dt;
    dt.year = 1979; dt.month = 5; dt.day = 27;
    dt.hour = 7; dt.minute = 32; dt.second = 0;
    dt.has_tz = true; dt.tz_minutes = 0;
    doc.root().set(R("t"), TomlValue::make_offset_datetime(dt));

    Utf8String out = write_str(doc);
    auto r = TomlReader::read(out.ref());
    ASSERT_TRUE(r.is_ok());
    TomlDocument back = std::move(r).unwrap();
    auto* v = back.root().find(R("t"));
    ASSERT_NE(v, nullptr);
    ASSERT_TRUE(v->is_offset_datetime());
    EXPECT_EQ(v->as_offset_datetime().year, 1979);
    EXPECT_EQ(v->as_offset_datetime().hour, 7);
    EXPECT_TRUE(v->as_offset_datetime().has_tz);
}

// ============================================================================
// Builder 自定义构造 + Writer round-trip 集成
// ============================================================================

TEST(TomlWriterTest, ComplexDocumentRoundTrip) {
    // 解析一段相对复杂的 TOML，再 write 出来，再解析回来，结构应一致。
    const char* text =
        "title = \"TOML Example\"\n"
        "\n"
        "[owner]\n"
        "name = \"Tom\"\n"
        "dob = 1979-05-27T07:32:00-08:00\n"
        "\n"
        "[database]\n"
        "enabled = true\n"
        "ports = [ 8001, 8001, 8002 ]\n"
        "\n"
        "[[servers]]\n"
        "ip = \"10.0.0.1\"\n"
        "[[servers]]\n"
        "ip = \"10.0.0.2\"\n";
    auto r1 = TomlReader::read(R(text));
    ASSERT_TRUE(r1.is_ok());
    TomlDocument doc1 = std::move(r1).unwrap();

    Utf8String out = write_str(doc1);
    auto r2 = TomlReader::read(out.ref());
    ASSERT_TRUE(r2.is_ok()) << "round-trip failed to reparse: " << out.c_str();
    TomlDocument doc2 = std::move(r2).unwrap();

    EXPECT_EQ(doc2.root().find(R("title"))->as_string(), R("TOML Example"));
    EXPECT_EQ(doc2.root().find(R("owner"))->find(R("name"))->as_string(), R("Tom"));
    EXPECT_TRUE(doc2.root().find(R("owner"))->find(R("dob"))->is_offset_datetime());
    EXPECT_EQ(doc2.root().find(R("database"))->find(R("ports"))->at(0).as_integer(), 8001);
    EXPECT_EQ(doc2.root().find(R("servers"))->size(), 2u);
    EXPECT_EQ(doc2.root().find(R("servers"))->at(1).find(R("ip"))->as_string(), R("10.0.0.2"));
}
