#pragma once

/// @file toml_datetime.hpp
/// @brief TOML datetime 数据模型：TomlDatetime + TomlDatetimeKind。
/// @details TOML 1.0 的 datetime 字面量有 4 种变体：offset date-time、local date-time、
///          local date、local time。本结构用 `TomlDatetimeKind` 枚举区分，避免拆成 4 个
///          独立的 DOM 分支。日期分量在 LocalDate 及以上变体有效，时间分量在 LocalTime 及以上
///          变体有效，时区仅在 OffsetDatetime 有效。

#include "libca/core/datatype.hpp"

namespace ca::toml {

/// @brief TOML datetime 的 4 种变体。
enum class TomlDatetimeKind
{
    OffsetDatetime,   ///< 日期 + 时间 + 时区。
    LocalDateTime,    ///< 日期 + 时间，无时区。
    LocalDate,        ///< 仅日期。
    LocalTime         ///< 仅时间。
};

/// @brief TOML datetime 数据。
/// @note 哪些字段有效取决于 `kind`：LocalDate 用年月日；LocalTime 用时分秒+纳秒；
///       LocalDateTime 用上述全部；OffsetDatetime 再加时区。
struct TomlDatetime
{
    /// 变体类型。
    TomlDatetimeKind kind = TomlDatetimeKind::LocalDate;

    // ---- 日期分量（LocalDate 及以上变体有效） ----
    /// 年（完整 4 位，如 1979）。
    ca::i32 year = 0;
    /// 月（1-12）。
    ca::u8 month = 0;
    /// 日（1-31）。
    ca::u8 day = 0;

    // ---- 时间分量（LocalTime 及以上变体有效） ----
    /// 时（0-23）。
    ca::u8 hour = 0;
    /// 分（0-59）。
    ca::u8 minute = 0;
    /// 秒（0-60，含闰秒）。
    ca::u8 second = 0;
    /// 小数秒纳秒（0-999999999）。
    ca::u32 nanos = 0;

    // ---- 时区（仅 OffsetDatetime 有效） ----
    /// 是否带时区。
    bool has_tz = false;
    /// 相对 UTC 的分钟偏移（东半球为正）。UTC 用 0 表示（不是 Z 的特殊标记）。
    ca::i16 tz_minutes = 0;
};

}   // namespace ca::toml
