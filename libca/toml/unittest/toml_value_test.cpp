#include "libca/toml/toml.hpp"

#include <gtest/gtest.h>

#include <utility>

using namespace ca;
using namespace ca::toml;
using ca::str::Utf8StringRef;
using ca::str::Utf8String;

namespace {
Utf8StringRef R(const char* s) { return Utf8StringRef::from_cstr(s); }
}  // namespace

// ============================================================================
// TomlValue 工厂与类型查询
// ============================================================================

TEST(TomlValueTest, FactoriesProduceCorrectType) {
    EXPECT_TRUE(TomlValue::make_string(R("x")).is_string());
    EXPECT_TRUE(TomlValue::make_integer(0).is_integer());
    EXPECT_TRUE(TomlValue::make_float(0.0).is_float());
    EXPECT_TRUE(TomlValue::make_boolean(true).is_boolean());
    EXPECT_TRUE(TomlValue::make_array().is_array());
    EXPECT_TRUE(TomlValue::make_table().is_table());
}

TEST(TomlValueTest, DefaultConstructIsTable) {
    TomlValue v;
    EXPECT_TRUE(v.is_table());
    EXPECT_EQ(v.type(), TomlType::Table);
}

TEST(TomlValueTest, DatetimeFactoriesSetKind) {
    TomlDatetime dt;
    dt.year = 2020; dt.month = 1; dt.day = 2;
    EXPECT_EQ(TomlValue::make_local_date(dt).as_local_date().kind, TomlDatetimeKind::LocalDate);
    EXPECT_EQ(TomlValue::make_local_datetime(dt).as_local_datetime().kind, TomlDatetimeKind::LocalDateTime);
    EXPECT_EQ(TomlValue::make_offset_datetime(dt).as_offset_datetime().kind, TomlDatetimeKind::OffsetDatetime);
    EXPECT_EQ(TomlValue::make_local_time(dt).as_local_time().kind, TomlDatetimeKind::LocalTime);
}

// ============================================================================
// Table / Array 编辑
// ============================================================================

TEST(TomlValueTest, TableSetAndFind) {
    TomlValue t = TomlValue::make_table();
    t.set(R("a"), TomlValue::make_integer(1));
    t.set(R("b"), TomlValue::make_integer(2));
    ASSERT_NE(t.find(R("a")), nullptr);
    EXPECT_EQ(t.find(R("a"))->as_integer(), 1);
    ASSERT_NE(t.find(R("b")), nullptr);
    EXPECT_EQ(t.find(R("b"))->as_integer(), 2);
    EXPECT_EQ(t.find(R("missing")), nullptr);
}

TEST(TomlValueTest, TableSetOverwritesSameKey) {
    TomlValue t = TomlValue::make_table();
    t.set(R("a"), TomlValue::make_integer(1));
    t.set(R("a"), TomlValue::make_integer(99));
    EXPECT_EQ(t.as_table().size(), 1u);
    EXPECT_EQ(t.find(R("a"))->as_integer(), 99);
}

TEST(TomlValueTest, TableRemove) {
    TomlValue t = TomlValue::make_table();
    t.set(R("a"), TomlValue::make_integer(1));
    EXPECT_TRUE(t.remove(R("a")));
    EXPECT_FALSE(t.remove(R("a")));
    EXPECT_EQ(t.find(R("a")), nullptr);
}

TEST(TomlValueTest, ArrayAppendAndAt) {
    TomlValue a = TomlValue::make_array();
    a.append(TomlValue::make_integer(1));
    a.append(TomlValue::make_integer(2));
    a.append(TomlValue::make_integer(3));
    EXPECT_EQ(a.size(), 3u);
    EXPECT_EQ(a.at(0).as_integer(), 1);
    EXPECT_EQ(a.at(2).as_integer(), 3);
}

// ============================================================================
// TomlValue 可拷贝（Utf8StringRef 可拷贝）—— 这是 Arena 架构的核心便利
// ============================================================================

TEST(TomlValueTest, ValueIsCopyable) {
    TomlValue original = TomlValue::make_string(R("hello"));
    TomlValue copy = original;  // 浅拷贝（共享 Utf8StringRef）
    EXPECT_EQ(copy.as_string(), R("hello"));
    EXPECT_EQ(original.as_string(), R("hello"));
}

TEST(TomlValueTest, TableKeyIsCopyable) {
    TomlValue t = TomlValue::make_table();
    t.set(R("key"), TomlValue::make_integer(1));
    TomlValue copy = t;  // 拷贝整张表
    EXPECT_NE(copy.find(R("key")), nullptr);
    EXPECT_EQ(copy.find(R("key"))->as_integer(), 1);
}

// ============================================================================
// Safe number conversion
// ============================================================================

TEST(TomlValueTest, AsFloatOrConvertsInteger) {
    TomlValue i = TomlValue::make_integer(42);
    EXPECT_DOUBLE_EQ(i.as_float_or(0.0), 42.0);
    TomlValue s = TomlValue::make_string(R("x"));
    EXPECT_DOUBLE_EQ(s.as_float_or(-1.0), -1.0);
}

TEST(TomlValueTest, AsIntegerOrTruncatesFloat) {
    TomlValue f = TomlValue::make_float(3.9);
    EXPECT_EQ(f.as_integer_or(0), 3);
}

// ============================================================================
// TomlDocument arena 集成
// ============================================================================

TEST(TomlDocumentTest, ArenaInternsStrings) {
    TomlDocument doc;
    Utf8StringRef r1 = doc.arena().intern(Utf8String::from_cstr("hello"));
    Utf8StringRef r2 = doc.arena().intern(Utf8String::from_cstr("hello"));
    EXPECT_EQ(r1.data(), r2.data());  // intern 去重：同一指针
}

TEST(TomlDocumentTest, RootIsTable) {
    TomlDocument doc;
    EXPECT_TRUE(doc.root().is_table());
}

TEST(TomlDocumentTest, MoveTransfersOwnership) {
    TomlDocument doc;
    doc.root().set(R("x"), TomlValue::make_integer(1));
    Utf8StringRef str_ref = doc.arena().intern(Utf8String::from_cstr("stored"));
    TomlDocument moved = std::move(doc);
    // moved 后原字符串引用仍指向新 arena
    EXPECT_EQ(moved.root().find(R("x"))->as_integer(), 1);
    EXPECT_NE(str_ref.data(), nullptr);
}
