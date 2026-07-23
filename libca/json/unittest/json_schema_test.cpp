/// @file json_schema_test.cpp
/// @brief JSON Schema 校验器测试：正反例覆盖各关键字。
/// schema 与 document 都从 JSON 文本解析构造，验证 type/required/properties/
/// const/enum/items/minimum/maximum/minLength/maxLength 与 schema 自身非法场景。

#include "libca/json/json.hpp"

#include <gtest/gtest.h>

#include <string>

using namespace ca::json;
using ca::str::Utf8StringRef;
using ca::str::Utf8String;

namespace {

Utf8StringRef R(const char* s) { return Utf8StringRef::from_cstr(s); }

JsonDocument read_ok(const char* text)
{
    auto result = JsonReader::read(R(text));
    EXPECT_TRUE(result.is_ok()) << "schema/doc parse should succeed";
    return std::move(result).unwrap();
}

std::string to_std(const Utf8String& s)
{
    return std::string(reinterpret_cast<const char*>(s.data()),
                       reinterpret_cast<const char*>(s.data()) + s.byte_length());
}

/// 校验并返回错误数；schema 非法时返回 -1。
int error_count(const char* doc_text, const char* schema_text)
{
    auto doc = read_ok(doc_text);
    auto schema = read_ok(schema_text);
    auto r = validate(doc.root(), schema.root());
    if (r.is_err()) return -1;
    return static_cast<int>(std::move(r).unwrap().size());
}

}  // namespace

// ==================== type ====================

TEST(JsonSchemaTest, Type_IntegerMatch)
{
    EXPECT_EQ(error_count("42", R"({"type":"integer"})"), 0);
}

TEST(JsonSchemaTest, Type_IntegerMismatch)
{
    EXPECT_EQ(error_count("\"hello\"", R"({"type":"integer"})"), 1);
}

TEST(JsonSchemaTest, Type_NumberAcceptsInteger)
{
    // draft 2020-12: number 接受整数。
    EXPECT_EQ(error_count("42", R"({"type":"number"})"), 0);
}

TEST(JsonSchemaTest, Type_NumberAcceptsFloat)
{
    EXPECT_EQ(error_count("3.14", R"({"type":"number"})"), 0);
}

TEST(JsonSchemaTest, Type_ArrayMultiMatch)
{
    // type 数组：integer 或 string 都接受。
    EXPECT_EQ(error_count("42", R"({"type":["integer","string"]})"), 0);
    EXPECT_EQ(error_count("\"x\"", R"({"type":["integer","string"]})"), 0);
}

TEST(JsonSchemaTest, Type_ArrayMultiMismatch)
{
    EXPECT_EQ(error_count("true", R"({"type":["integer","string"]})"), 1);
}

TEST(JsonSchemaTest, Type_ObjectAndArray)
{
    EXPECT_EQ(error_count("{}", R"({"type":"object"})"), 0);
    EXPECT_EQ(error_count("[]", R"({"type":"array"})"), 0);
    EXPECT_EQ(error_count("{}", R"({"type":"array"})"), 1);
}

TEST(JsonSchemaTest, Type_NullAndBoolean)
{
    EXPECT_EQ(error_count("null", R"({"type":"null"})"), 0);
    EXPECT_EQ(error_count("true", R"({"type":"boolean"})"), 0);
    EXPECT_EQ(error_count("null", R"({"type":"boolean"})"), 1);
}

// ==================== const ====================

TEST(JsonSchemaTest, Const_IntegerEqual)
{
    EXPECT_EQ(error_count("2", R"({"const":2})"), 0);
}

TEST(JsonSchemaTest, Const_IntegerNotEqual)
{
    EXPECT_EQ(error_count("3", R"({"const":2})"), 1);
}

TEST(JsonSchemaTest, Const_StringEqual)
{
    EXPECT_EQ(error_count("\"v2\"", R"({"const":"v2"})"), 0);
}

TEST(JsonSchemaTest, Const_ObjectEqual)
{
    EXPECT_EQ(error_count(R"({"a":1})", R"({"const":{"a":1}})"), 0);
    EXPECT_EQ(error_count(R"({"a":2})", R"({"const":{"a":1}})"), 1);
}

// ==================== enum ====================

TEST(JsonSchemaTest, Enum_Match)
{
    EXPECT_EQ(error_count("\"b\"", R"({"enum":["a","b","c"]})"), 0);
    EXPECT_EQ(error_count("2", R"({"enum":[1,2,3]})"), 0);
}

TEST(JsonSchemaTest, Enum_NoMatch)
{
    EXPECT_EQ(error_count("\"d\"", R"({"enum":["a","b","c"]})"), 1);
}

// ==================== required / properties ====================

TEST(JsonSchemaTest, Required_AllPresent)
{
    EXPECT_EQ(error_count(R"({"a":1,"b":2})", R"({"required":["a","b"]})"), 0);
}

TEST(JsonSchemaTest, Required_Missing)
{
    EXPECT_EQ(error_count(R"({"a":1})", R"({"required":["a","b"]})"), 1);
}

TEST(JsonSchemaTest, Properties_NestedTypeMatch)
{
    const char* schema = R"({"properties":{"classes":{"type":"integer"},"name":{"type":"string"}}})";
    EXPECT_EQ(error_count(R"({"classes":1,"name":"x"})", schema), 0);
}

TEST(JsonSchemaTest, Properties_NestedTypeMismatch)
{
    const char* schema = R"({"properties":{"classes":{"type":"integer"}}})";
    EXPECT_EQ(error_count(R"({"classes":"oops"})", schema), 1);
}

TEST(JsonSchemaTest, Properties_AbsentFieldNotChecked)
{
    // 缺失字段由 required 管；properties 只校验存在的字段。
    const char* schema = R"({"properties":{"classes":{"type":"integer"}}})";
    EXPECT_EQ(error_count("{}", schema), 0);
}

TEST(JsonSchemaTest, Properties_DeepNestedErrorPath)
{
    // 校验深层嵌套：before/classes 必须是 integer。
    const char* schema = R"({"properties":{"before":{"properties":{"classes":{"type":"integer"}}}}})";
    EXPECT_EQ(error_count(R"({"before":{"classes":5}})", schema), 0);
    EXPECT_EQ(error_count(R"({"before":{"classes":"bad"}})", schema), 1);
}

// ==================== items ====================

TEST(JsonSchemaTest, Items_AllMatch)
{
    EXPECT_EQ(error_count("[1,2,3]", R"({"items":{"type":"integer"}})"), 0);
}

TEST(JsonSchemaTest, Items_OneMismatch)
{
    EXPECT_EQ(error_count("[1,\"x\",3]", R"({"items":{"type":"integer"}})"), 1);
}

TEST(JsonSchemaTest, Items_MultipleMismatch)
{
    // 默认收集所有失败。
    EXPECT_EQ(error_count("[\"a\",\"b\"]", R"({"items":{"type":"integer"}})"), 2);
}

// ==================== numeric bounds ====================

TEST(JsonSchemaTest, Minimum_WithinRange)
{
    EXPECT_EQ(error_count("5", R"({"minimum":1,"maximum":10})"), 0);
}

TEST(JsonSchemaTest, Minimum_BelowLower)
{
    EXPECT_EQ(error_count("0", R"({"minimum":1})"), 1);
}

TEST(JsonSchemaTest, Maximum_AboveUpper)
{
    EXPECT_EQ(error_count("11", R"({"maximum":10})"), 1);
}

TEST(JsonSchemaTest, Bounds_BoundaryInclusive)
{
    // 闭区间：等于边界值合法。
    EXPECT_EQ(error_count("1", R"({"minimum":1,"maximum":1})"), 0);
}

// ==================== string bounds ====================

TEST(JsonSchemaTest, MinLength_LongEnough)
{
    EXPECT_EQ(error_count("\"abc\"", R"({"minLength":3})"), 0);
}

TEST(JsonSchemaTest, MinLength_TooShort)
{
    EXPECT_EQ(error_count("\"ab\"", R"({"minLength":3})"), 1);
}

TEST(JsonSchemaTest, MaxLength_NotTooLong)
{
    EXPECT_EQ(error_count("\"abc\"", R"({"maxLength":3})"), 0);
}

TEST(JsonSchemaTest, MaxLength_TooLong)
{
    EXPECT_EQ(error_count("\"abcd\"", R"({"maxLength":3})"), 1);
}

TEST(JsonSchemaTest, StringBounds_CountsCodePoints)
{
    // 中文每个字符算 1 码点。"中文" = 2 码点。
    EXPECT_EQ(error_count(R"("中文")", R"({"minLength":2})"), 0);
    EXPECT_EQ(error_count(R"("中")", R"({"minLength":2})"), 1);
}

// ==================== 综合与契约场景 ====================

TEST(JsonSchemaTest, Combined_ContractLikeSchema)
{
    // 模拟 morpher release contract 的 schema 形态。
    const char* schema = R"({
        "type":"object",
        "required":["schema_version","before","after"],
        "properties":{
            "schema_version":{"type":"integer","const":1},
            "before":{"type":"object","required":["classes"]},
            "after":{"type":"object","required":["classes"]}
        }
    })";
    // 合法文档。
    EXPECT_EQ(error_count(R"({"schema_version":1,"before":{"classes":10},"after":{"classes":8}})", schema), 0);
    // schema_version 错值。
    EXPECT_EQ(error_count(R"({"schema_version":2,"before":{"classes":10},"after":{"classes":8}})", schema), 1);
    // 缺 before。
    EXPECT_EQ(error_count(R"({"schema_version":1,"after":{"classes":8}})", schema), 1);
}

TEST(JsonSchemaTest, MultipleErrorsCollected)
{
    // 默认收集所有失败：两处字段类型错。
    const char* schema = R"({"properties":{"a":{"type":"integer"},"b":{"type":"string"}}})";
    EXPECT_EQ(error_count(R"({"a":"x","b":1})", schema), 2);
}

// ==================== schema 自身非法 ====================

TEST(JsonSchemaTest, SchemaError_UnknownType)
{
    auto schema = read_ok(R"({"type":"integre"})");
    auto doc = read_ok("42");
    auto r = validate(doc.root(), schema.root());
    EXPECT_TRUE(r.is_err());
}

TEST(JsonSchemaTest, SchemaError_TypeNotStringOrArray)
{
    auto schema = read_ok(R"({"type":123})");
    auto doc = read_ok("42");
    auto r = validate(doc.root(), schema.root());
    EXPECT_TRUE(r.is_err());
}

TEST(JsonSchemaTest, SchemaError_RootNotObject)
{
    auto schema = read_ok(R"("not an object")");
    auto doc = read_ok("42");
    auto r = validate(doc.root(), schema.root());
    EXPECT_TRUE(r.is_err());
}

TEST(JsonSchemaTest, SchemaError_PropertiesNotObject)
{
    auto schema = read_ok(R"({"properties":[1,2]})");
    auto doc = read_ok("{}");
    auto r = validate(doc.root(), schema.root());
    EXPECT_TRUE(r.is_err());
}

// ==================== instance_path / message 结构 ====================

TEST(JsonSchemaTest, ErrorCarriesInstancePath)
{
    auto doc = read_ok(R"({"before":{"classes":"bad"}})");
    auto schema = read_ok(R"({"properties":{"before":{"properties":{"classes":{"type":"integer"}}}}})");
    auto r = validate(doc.root(), schema.root());
    ASSERT_TRUE(r.is_ok());
    auto errors = std::move(r).unwrap();
    ASSERT_EQ(errors.size(), 1u);
    const auto& err = errors[0];
    EXPECT_EQ(to_std(err.instance_path), "/before/classes");
    EXPECT_NE(to_std(err.message).find("integer"), std::string::npos);
}
