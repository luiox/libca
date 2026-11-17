.. _time-design:

libca::time 设计文档
====================

本文讲 time 模块的设计边界。具体 API 使用请查看 ``duration.hpp``\ 、``timestamp.hpp``\ 、``datetime.hpp``\ 。

模块定位
-----------

``libca::time`` 提供基础时间类型，用于日志、缓存过期、调度、状态文件和跨模块时间计算。它依赖 core 的定长类型，但不依赖 fs、str、crypto 等上层模块。

模块包含两类能力：

- **Duration / Timestamp**：轻量纯值类型，适合程序内部计算。
- **Date / Time / DateTime**：面向日历展示和简单解析的旧接口。

此外还提供两个基于 ``TimeUtil::nano_time()``\ （steady_clock）的测量入口：
``Stopwatch`` 单调秒表与 ``ScopeTiming`` RAII 耗时打点，见 :ref:`time-design-stopwatch`。

Duration
-----------

``Duration`` 表示纳秒精度的时间间隔，内部只保存一个 ``i64`` 纳秒值。

设计原则：

- 纯值类型，无系统调用。
- 支持负数，用于表达两个时间戳的差值。
- 单位工厂统一转换到纳秒，避免多字段归一化复杂度。
- 算术、比较、单位读取和 chrono 转换均支持 ``constexpr``。

``Duration`` 不负责格式化，也不负责“月份”“年份”这类日历相关间隔，因为这些单位依赖日历规则和时区。

Timestamp
------------

``Timestamp`` 表示 Unix epoch 起算的纳秒时间戳，内部同样只保存一个 ``i64`` 纳秒值。

设计原则：

- epoch 语义明确，便于序列化和跨平台传输。
- 与 ``Duration`` 做加减，两个 ``Timestamp`` 相减得到 ``Duration``。
- 与 ``std::chrono::system_clock::time_point`` 互转，作为标准库边界。
- 除 ``now()``、``is_past()``、``is_future()`` 外，其它操作均为纯值计算并支持 ``constexpr``。

``Timestamp`` 不包含时区、夏令时或本地化显示信息。需要这些能力时应在更高层或专门的 datetime 组件中处理。

.. _time-design-clock:

chrono 边界
--------------

time 模块接受 ``std::chrono`` 作为 C++ 标准库互操作层，但不直接暴露多种内部表示：

- ``Duration::from_chrono`` 接受任意 ``std::chrono::duration``，内部转换为纳秒。
- ``Duration::to_chrono`` 返回 ``std::chrono::nanoseconds``。
- ``Timestamp::from_time_point`` 限定为 ``system_clock``，避免混用 steady clock 等无 epoch 语义的时钟。
- ``Timestamp::to_time_point`` 返回 ``system_clock::time_point``。

checked chrono 入口接收不超过 64 位的内置整数计数，包括无符号计数。
转换先得到整数秒与有理余数，再精确换算纳秒，小于一纳秒的部分向零截断。
中间商余数使用逐位整数算法，避免先乘再检查，也避免巨大 Period 在 ratio 换算时发生编译期溢出。
浮点 Rep 继续通过既有入口接入，其有限性和目标范围由调用方保证。

这种设计让 libca 的内部时间语义保持简单，同时不阻断调用方与标准库 API 交互。

.. _time-design-overflow:

错误与溢出策略
-----------------

``Duration`` 与 ``Timestamp`` 使用完整的有符号 64 位纳秒范围。
同一个合法值在加减、倍乘或时间戳求差后可能超出这一范围。

保留原有单位工厂、算术运算符与 chrono 转换的 ``constexpr`` 入口及向零截断语义。
调用这些入口时，调用方负责保证结果与内部乘法均可表示。最小负纳秒值取相反数也会超出范围。
处理外部输入或组合多个时间值时，使用独立的 ``checked_*`` 入口检查边界，失败返回空值。

checked 入口采用 ``std::optional``，让检查、成功值读取和失败分支都可在 C++17 常量求值中完成。
core 当前的 ``Option`` 与 ``Result`` 构造和访问尚未支持这一契约，因此此处沿用仓库已有的标准库可空值表达。

单位工厂在乘法前检查商边界，加减在求值前检查符号和余量。
倍乘按无符号幅值检查，覆盖最小负值，避免先取负再检查的溢出路径。
时间戳的构造、偏移和求差复用相同的纳秒检查。

.. _time-design-negative-epoch:

epoch 前的整单位读取
----------------------

现有 ``unix_seconds`` 与 ``unix_milliseconds`` 保持向零截断。
对于 epoch 前一纳秒，这两个值均为零。

新增显式的 floor 读取向负无穷取整，以表达该时间点所在的 Unix 秒或毫秒区间。
epoch 前一纳秒落在索引为负一的秒与毫秒内。整秒、整毫秒边界保持原值。
实现通过商与余数调整，最小负纳秒值也无需取相反数。

测试策略
-----------

测试位于 ``libca/time/unittest/``。重点覆盖：

- 单位工厂与单位读取。
- Duration/Timestamp 算术和比较。
- chrono roundtrip、整数有理单位转换与亚纳秒截断。
- ``now()`` 的粗略合理性。
- 纯值操作的 ``constexpr`` 可用性。
- checked 工厂与算术在正负端点的成功及溢出分支。
- epoch 前整单位截断与 floor 的边界差异。

Date / Time / DateTime
-------------------------

``Date``、``Time`` 和 ``DateTime`` 是面向简单日历展示的接口：

- ``Date`` 保存年、月、日，支持从 ``"YYYY-MM-DD"`` 字符串构造并格式化回字符串。
- ``Time`` 保存时、分、秒，支持从 ``"HH:MM:SS"`` 字符串构造并格式化回字符串。
- ``DateTime`` 提供获取当前本地日期和时间的静态入口。

这些类型不承载时区、闰秒、日历系统转换等复杂语义，也不应替代 ``Timestamp`` 表达绝对时间点。

.. _time-design-stopwatch:

Stopwatch 与 ScopeTiming
---------------------------

``Stopwatch`` 与 ``ScopeTiming`` 是两个只测相对耗时的测量入口，统一走 ``TimeUtil::nano_time()`` （steady_clock），不引入 wall clock，避免系统对时干扰测量。

- ``Stopwatch`` 是纯表示类：构造即启动，只保存一个起点。``restart()`` 把起点移到当前时刻重新计时；``reset()`` 进入停表态，``elapsed()`` 冻结为 0 直到下次 ``restart()``。不提供 start/stop 累计语义，保持“测一段代码耗时”的最小用法。
- ``ScopeTiming`` 是 RAII guard：构造登记名字并取起点，析构把耗时累计进注册表。聚合统计（``count/total/max``）放在 ``ScopeTimingRegistry``，以 std::mutex 保护，guard 本身不加锁。全局默认注册表是进程级单例，测试注入独立注册表即可精确断言、互不污染。

测量入口内部继续使用原有轻量值运算，要求累计统计量可由有符号 64 位纳秒表示。

新人阅读顺序
---------------

建议先看 ``duration.hpp`` 和 ``timestamp.hpp`` 理解纯值时间模型，再看 ``datetime.hpp`` 理解旧的日历展示接口。对应测试位于 ``libca/time/unittest/checked_time_test.cpp``、``libca/time/unittest/duration_timestamp_test.cpp`` 和 ``libca/time/unittest/datetime_test.cpp``。
