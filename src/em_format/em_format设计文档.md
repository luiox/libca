# em_format 设计文档

## 1. 设计目标

`em_format` 提供一个面向嵌入式场景的轻量格式化模块，目标如下：

1. **低体积**：默认最小功能集，不为未使用能力付出 ROM 成本。
2. **可裁剪**：通过编译宏按需启用 `%f`、`%x/%X`、宽度/精度解析。
3. **可预测**：接口返回值和截断行为明确，支持标准语义与 fast 语义并存。
4. **可复用**：从 `simple_logger` 抽离后成为通用基础能力，供其他模块复用。

---

## 2. 模块边界与目录

- 核心实现：`src/em_format/format.c`
- 对外头文件：`src/em_format/format.h`
- 构建配置：`src/em_format/xmake.lua`
- 兼容入口：`src/em_base/format.h`（仅转发到 `em_format/format.h`）

边界约束：
- `em_format` 仅负责字符串数值转换与轻量格式化；
- 不引入动态内存；
- 不依赖标准 `printf` 实现。

---

## 3. API 设计

### 3.1 转换函数

- `u32_to_str / u32_to_str_safe`
- `f32_to_str / f32_to_str_safe`
- `f64_to_str / f64_to_str_safe`

设计要点：
- safe 版本全部显式接收 `buf_len`。
- 发生截断时保证 `\0` 终止（`buf_len > 0`）。
- 返回值统一为“期望写入长度（不含终止符）”，便于上层判断是否截断。

### 3.2 格式化函数

- 标准语义：`fmt_snprintf / fmt_vsnprintf`
- fast 语义：`fmt_snprintf_fast / fmt_vsnprintf_fast`
- 无边界版本：`fmt_sprintf`

设计要点：
- 标准语义对齐 `snprintf`：返回“本应写入长度”。
- fast 语义在截断时返回“实际写入长度”，用于日志等快速路径。
- `fmt_vsnprintf_fast` 提供 `va_list` 直通能力，便于包装层复用。

---

## 4. 格式能力与降级策略

### 4.1 支持的格式符

基础默认：
- `%d` `%u` `%s` `%%`

可选能力：
- `%x` `%X`（`FMT_ENABLE_HEX=1`）
- `%f`（`FMT_ENABLE_FLOAT=1`）
- 宽度/精度（`FMT_ENABLE_WIDTH_PRECISION=1`）

### 4.2 不支持能力的行为

当遇到未启用/不支持格式符时，按“`%` + 原字符”直接输出。
该策略用于：
- 保持行为可预测；
- 避免 silent drop；
- 简化上层排障。

---

## 5. 浮点策略与代码体积

为兼顾体积与行为，浮点采用编译期策略选择：

- `FMT_FLOAT_MODE_FIXED`
  - 固定小数位（`FMT_FIXED_DECIMALS`）
  - 计算路径最简单，便于控制体积
- `FMT_FLOAT_MODE_SIMPLE`
  - 支持请求精度（或默认精度）
  - 使用简化截断策略
- `FMT_FLOAT_MODE_NORMAL`
  - 支持请求精度（或默认精度）
  - 提供常规舍入语义

实现层面将 SIMPLE/FIXED 与 NORMAL 保持清晰路径分离，减少不必要逻辑耦合，方便编译器做死代码裁剪。

---

## 6. 安全性与鲁棒性

模块对关键边界进行约束：

1. `buf == NULL` / `fmt == NULL` 的防御处理。
2. `buf_size == 0` 时不写内存但返回可用于判断的长度语义。
3. 任何截断路径保证 `buf` 末尾可终止（当 `buf_size > 0`）。
4. safe 转换接口在极小缓冲区下保持确定行为。

---

## 7. 测试设计

`em_format/xmake.lua` 提供测试矩阵：

- `test-format`：默认配置（最小特性）
- `test-format-float-fixed`
- `test-format-float-simple`
- `test-format-float-normal`

测试覆盖重点：
- 标准/fast 返回语义差异；
- 截断与 `\0` 终止；
- NULL/空缓冲区分支；
- 功能开关开启/关闭后的可见行为；
- 不支持格式符回退输出。

---

## 8. 与 em_log 的关系

`simple_logger` 复用 `em_format`，不再维护独立格式实现。这样可实现：

- 统一格式行为与 bugfix 入口；
- logger fast 路径与 format fast 语义一致；
- 编译宏在模块间保持一致裁剪策略。

---

## 9. 兼容与迁移策略

为降低改造成本，保留：

- `src/em_base/format.h` 作为兼容 shim（转发到新路径）。

推荐：
- 新代码直接包含 `../em_format/format.h`；
- 旧代码可逐步迁移，不要求一次性改完。

---

## 10. 后续演进建议

1. 继续按特性宏细分解析器分支，进一步缩小最小构建体积。
2. 为不同芯片/编译器记录二进制体积对比基线。
3. 若未来引入更多格式符，优先保持“默认关闭、按需开启”的原则。
