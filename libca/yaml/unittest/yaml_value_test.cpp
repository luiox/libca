#include <gtest/gtest.h>

#include "libca/yaml/yaml_document.hpp"
#include "libca/yaml/yaml_value.hpp"

#include "libca/str/utf8_string.hpp"

using namespace ca;
using namespace ca::yaml;
using ca::str::Utf8StringRef;

namespace {

Utf8StringRef R(const char* s)
{
    return Utf8StringRef::from_cstr(s);
}

}   // namespace

// ============================================================================
// 类型谓词与工厂
// ============================================================================

TEST(YamlValueTest, DefaultIsNull)
{
    YamlValue v;
    EXPECT_TRUE(v.is_null());
    EXPECT_EQ(v.type(), YamlType::Null);
}

TEST(YamlValueTest, FactoriesAndPredicates)
{
    EXPECT_TRUE(YamlValue::make_null().is_null());
    EXPECT_TRUE(YamlValue::make_boolean(true).is_boolean());
    EXPECT_TRUE(YamlValue::make_integer(42).is_integer());
    EXPECT_TRUE(YamlValue::make_float(3.14).is_float());
    EXPECT_TRUE(YamlValue::make_string(R("x")).is_string());
    EXPECT_TRUE(YamlValue::make_sequence().is_sequence());
    EXPECT_TRUE(YamlValue::make_mapping().is_mapping());
}

TEST(YamlValueTest, ScalarAccessors)
{
    EXPECT_EQ(YamlValue::make_boolean(true).as_boolean(), true);
    EXPECT_EQ(YamlValue::make_integer(-7).as_integer(), -7);
    EXPECT_DOUBLE_EQ(YamlValue::make_float(2.5).as_float(), 2.5);
    EXPECT_EQ(YamlValue::make_string(R("hello")).as_string(), R("hello"));
}

TEST(YamlValueTest, NumericCoercion)
{
    EXPECT_DOUBLE_EQ(YamlValue::make_integer(3).as_float_or(0.0), 3.0);
    EXPECT_EQ(YamlValue::make_float(3.9).as_integer_or(0), 3);
    EXPECT_EQ(YamlValue::make_string(R("x")).as_integer_or(-1), -1);
    EXPECT_DOUBLE_EQ(YamlValue::make_null().as_float_or(1.5), 1.5);
}

// ============================================================================
// Sequence
// ============================================================================

TEST(YamlValueTest, SequenceAppendAtSize)
{
    auto seq = YamlValue::make_sequence();
    EXPECT_EQ(seq.size(), 0u);
    seq.append(YamlValue::make_integer(1));
    seq.append(YamlValue::make_string(R("two")));
    seq.append(YamlValue::make_null());
    ASSERT_EQ(seq.size(), 3u);
    EXPECT_EQ(seq.at(0).as_integer(), 1);
    EXPECT_EQ(seq.at(1).as_string(), R("two"));
    EXPECT_TRUE(seq.at(2).is_null());
}

// ============================================================================
// Mapping
// ============================================================================

TEST(YamlValueTest, MappingSetFindOrder)
{
    auto map = YamlValue::make_mapping();
    map.set(R("b"), YamlValue::make_integer(2));
    map.set(R("a"), YamlValue::make_integer(1));
    map.set(R("c"), YamlValue::make_integer(3));

    // 保插入序
    const auto& members = map.as_mapping();
    ASSERT_EQ(members.size(), 3u);
    EXPECT_EQ(members[0].first, R("b"));
    EXPECT_EQ(members[1].first, R("a"));
    EXPECT_EQ(members[2].first, R("c"));

    // O(1) 查找
    const auto* v = map.find(R("a"));
    ASSERT_NE(v, nullptr);
    EXPECT_EQ(v->as_integer(), 1);
    EXPECT_EQ(map.find(R("missing")), nullptr);

    // set 覆盖同名 key 不改变位置
    map.set(R("a"), YamlValue::make_string(R("new")));
    ASSERT_EQ(map.as_mapping().size(), 3u);
    EXPECT_EQ(map.as_mapping()[1].second.as_string(), R("new"));
}

TEST(YamlValueTest, MappingRemoveKeepsIndexConsistent)
{
    auto map = YamlValue::make_mapping();
    map.set(R("a"), YamlValue::make_integer(1));
    map.set(R("b"), YamlValue::make_integer(2));
    map.set(R("c"), YamlValue::make_integer(3));

    EXPECT_TRUE(map.remove(R("b")));
    EXPECT_FALSE(map.remove(R("b")));
    ASSERT_EQ(map.as_mapping().size(), 2u);
    // 移除后索引仍指向正确成员
    const auto* c = map.find(R("c"));
    ASSERT_NE(c, nullptr);
    EXPECT_EQ(c->as_integer(), 3);
    const auto* a = map.find(R("a"));
    ASSERT_NE(a, nullptr);
    EXPECT_EQ(a->as_integer(), 1);
}

// ============================================================================
// clone / document
// ============================================================================

TEST(YamlValueTest, CloneIsDeep)
{
    auto map = YamlValue::make_mapping();
    auto seq = YamlValue::make_sequence();
    seq.append(YamlValue::make_integer(1));
    map.set(R("list"), std::move(seq));

    YamlValue copy = map.clone();
    // 修改副本不影响原件
    copy.find(R("list"))->append(YamlValue::make_integer(2));
    EXPECT_EQ(copy.find(R("list"))->size(), 2u);
    EXPECT_EQ(map.find(R("list"))->size(), 1u);
}

TEST(YamlDocumentTest, RootDefaultsToNullAndClear)
{
    YamlDocument doc;
    EXPECT_TRUE(doc.root().is_null());

    doc.root() = YamlValue::make_mapping();
    doc.root().set(doc.arena().intern("key"), YamlValue::make_integer(1));
    EXPECT_TRUE(doc.root().is_mapping());

    doc.clear();
    EXPECT_TRUE(doc.root().is_null());
}

TEST(YamlDocumentTest, MoveTransfersOwnership)
{
    YamlDocument doc;
    doc.root() = YamlValue::make_mapping();
    doc.root().set(doc.arena().intern("k"), YamlValue::make_string(doc.arena().intern("v")));

    YamlDocument moved = std::move(doc);
    const auto*  v     = moved.root().find(R("k"));
    ASSERT_NE(v, nullptr);
    EXPECT_EQ(v->as_string(), R("v"));
}
