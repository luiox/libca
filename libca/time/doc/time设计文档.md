---
version: 1.1
update:
2026-07-14 - 删除冗余英文摘要 design.md，本文成为 time 唯一设计文档
2026-07-06 - 首版，补充 time 模块职责、Duration/Timestamp 值语义与 chrono 边界
---

# libca::time 设计文档

> 本文讲 time 模块的设计边界。具体 API 使用请查看 `duration.hpp`、`timestamp.hpp`、`datetime.hpp`。

## 1. 模块定位

`libca::time` 提供基础时间类型，用于日志、缓存过期、调度、状态文件和跨模块时间计算。它依赖 core 的定长类型，但不依赖 fs、str、crypto 等上层模块。

模块包含两类能力：

- **Duration / Timestamp**：轻量纯值类型，适合程序内部计算。
- **Date / Time / DateTime**：面向日历展示和简单解析的旧接口。

## 2. Duration

`Duration` 表示纳秒精度的时间间隔，内部只保存一个 `i64` 纳秒值。

设计原则：

- 纯值类型，无系统调用。
- 支持负数，用于表达两个时间戳的差值。
- 单位工厂统一转换到纳秒，避免多字段归一化复杂度。
- 算术、比较、单位读取和 chrono 转换均支持 `constexpr`。

`Duration` 不负责格式化，也不负责“月份”“年份”这类日历相关间隔，因为这些单位依赖日历规则和时区。

## 3. Timestamp

`Timestamp` 表示 Unix epoch 起算的纳秒时间戳，内部同样只保存一个 `i64` 纳秒值。

设计原则：

- epoch 语义明确，便于序列化和跨平台传输。
- 与 `Duration` 做加减，两个 `Timestamp` 相减得到 `Duration`。
- 与 `std::chrono::system_clock::time_point` 互转，作为标准库边界。
- 除 `now()`、`is_past()`、`is_future()` 外，其它操作均为纯值计算并支持 `constexpr`。

`Timestamp` 不包含时区、夏令时或本地化显示信息。需要这些能力时应在更高层或专门的 datetime 组件中处理。

## 4. chrono 边界

time 模块接受 `std::chrono` 作为 C++ 标准库互操作层，但不直接暴露多种内部表示：

- `Duration::from_chrono` 接受任意 `std::chrono::duration`，内部转换为纳秒。
- `Duration::to_chrono` 返回 `std::chrono::nanoseconds`。
- `Timestamp::from_time_point` 限定为 `system_clock`，避免混用 steady clock 等无 epoch 语义的时钟。
- `Timestamp::to_time_point` 返回 `system_clock::time_point`。

这种设计让 libca 的内部时间语义保持简单，同时不阻断调用方与标准库 API 交互。

## 5. 错误与溢出策略

当前 `Duration` / `Timestamp` 是低层值类型，不做运行时溢出检查。调用方应根据业务场景控制输入范围。

原因是：

- 时间运算通常处在高频路径，低层类型应保持轻量。
- C++ 有符号整数溢出本身是调用方前置条件问题。
- 需要饱和运算或 checked arithmetic 时，应作为单独 API 明确暴露。

## 6. 测试策略

测试位于 `libca/time/unittest/`。重点覆盖：

- 单位工厂与单位读取。
- Duration/Timestamp 算术和比较。
- chrono roundtrip。
- `now()` 的粗略合理性。
- 纯值操作的 `constexpr` 可用性。

## 7. Date / Time / DateTime

`Date`、`Time` 和 `DateTime` 是面向简单日历展示的接口：

- `Date` 保存年、月、日，支持从 `"YYYY-MM-DD"` 字符串构造并格式化回字符串。
- `Time` 保存时、分、秒，支持从 `"HH:MM:SS"` 字符串构造并格式化回字符串。
- `DateTime` 提供获取当前本地日期和时间的静态入口。

这些类型不承载时区、闰秒、日历系统转换等复杂语义，也不应替代 `Timestamp` 表达绝对时间点。

## 8. 新人阅读顺序

建议先看 `duration.hpp` 和 `timestamp.hpp` 理解纯值时间模型，再看 `datetime.hpp` 理解旧的日历展示接口。对应测试位于 `libca/time/unittest/duration_timestamp_test.cpp` 和 `libca/time/unittest/datetime_test.cpp`。
