#pragma once

/// @file yaml_scalar.hpp
/// @brief YAML plain 标量的类型判定（YAML 1.2 core schema），parser 与 writer 共用。
/// @details parser 用它把未加引号的标量文本解析成 Null/Boolean/Integer/Float/String；
///          writer 用它判定一个字符串**若不加引号写出**会被读成什么类型——
///          kind != String 时必须加引号才能保住字符串身份（如 "true"、"3.14"）。
/// @note 只认 true/false（含 True/TRUE 变体），不认 yes/no/on/off（Norway problem）。

#include "libca/core/datatype.hpp"

namespace ca::yaml {

/// @brief plain 标量的判定结果类别。
enum class PlainScalarKind
{
    Null,           ///< 空 / ~ / null / Null / NULL
    Boolean,        ///< true/false（含首字母/全大写变体）
    Integer,        ///< 十进制（可带符号）/ 0x 十六进制 / 0o 八进制
    Float,          ///< 十进制浮点 / 科学计数 / .inf / .nan 家族
    String,         ///< 其余一切
    IntOverflow,    ///< 形如整数但超出 i64 —— parser 应报错，writer 应加引号
    FloatOverflow   ///< 形如浮点但超出 f64 范围
};

/// @brief plain 标量判定结果：类别 + 对应值（仅对应类别的字段有效）。
struct ResolvedScalar
{
    PlainScalarKind kind     = PlainScalarKind::String;
    bool            boolean  = false;
    ca::i64         integer  = 0;
    ca::f64         floating = 0.0;
};

/// @brief 按 YAML 1.2 core schema 判定一段（已去首尾空白的）plain 标量文本。
/// @param data 文本字节（可为 nullptr 当 len 为 0）。
/// @param len 字节长度。
ResolvedScalar resolve_plain_scalar(const u8* data, usize len) noexcept;

}   // namespace ca::yaml
