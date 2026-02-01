# 03_develop_workflow — 驱动开发流程与操作指南

本文件为驱动开发的逐步操作指南，侧重可复用的流程与交付物（代码、测试、文档）。为了避免重复，本指南大量引用模板文件，请在实现时复用 `.codebuddy/skills/em-driver-dev/assets/` 下的模板和检查表。

## 流程概览（高层）
1. 分析硬件（通信接口、port 函数、配置、时序约束）
2. 定义接口（port 层、device object、API）
3. 实现驱动（port 绑定、Access 宏、API 实现）
4. 集成到构建系统（xmake）并添加测试目标（如适用）
5. 测试与验证（模拟测试 → 硬件验证）
6. 提交 PR 并附上验证材料

---

## Step 1 — 硬件分析（必做）
在开始编码前，收集并记录以下信息：
- 接口类型（GPIO/I2C/SPI/UART/自定义时序）
- 必要的 Port 函数（读/写、延时、配置模式切换等）
- 时序约束（最小/最大延时、超时值）
- 物理参数（电源电压、接口电平、上拉/下拉要求）
- 地址/寄存器映射、默认配置和校准参数

把这些内容记录在驱动的规范段落中（建议在 PR 描述或单独的 `driver_testing_plan.md` 中引用）。模板路径：
- `assets/driver_testing_plan.md`

---

## Step 2 — 定义接口（Port 与 Device）
- 使用 `void*` 作为硬件句柄以保持平台无关性。
- Port 结构只包含最少必要函数，按功能分组（GPIO、I2C、时序等）。
- Device 对象（`xxx_t`）包含硬件句柄、配置项与状态变量，状态变量放在结构体末尾。

建议使用模板创建头文件：
- 参考：`.codebuddy/skills/em-driver-dev/assets/driver_header_template.h.md`

设计约定（摘要）：
- 所有对外 API 第一个参数为 `xxx_t* self`。
- 当 `xxx_t` 成员较多（建议阈值 > 4）时，硬件字段由调用者在外部初始化（结构体字面量或逐项赋值），`xxx_init()` 仅接收配置参数并做最小初始化。

---

## Step 3 — 实现驱动
- 在实现文件中定义全局 Port 指针与绑定函数：
  ```c
  static const xxx_port_t* g_xxx_port = NULL;
  void xxx_bind_port(const xxx_port_t* port) { g_xxx_port = port; }
  ```
- 使用 Access 宏封装常用硬件操作，例：`#define XXX_WRITE(self, v) g_xxx_port->write((self)->gpio, (self)->pin, (v))`。
- 所有外部 API 要使用 `param_check` 对 `self` 等关键参数做契约式检查；对其它参数则按需用 if 判断返回错误码。
- 日志必须使用 `em_base/debug.h`（禁止 `printf`）。

实现示例与风格请参考：
- `assets/driver_c_template.c.md`

