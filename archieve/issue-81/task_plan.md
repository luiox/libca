# Task Plan (Issue #81)

## 状态
- 当前状态: 进行中（验收前）
- issue: https://github.com/luiox/libca/issues/81
- 最后更新: 2026-04-01

## 需求来源摘要
- 标题: [libca] 内存操作和字符串操作改进
- 目标: 为 memory_util 与 string_util 增加标准库/内置实现切换能力。
- 默认行为: 未显式配置时，使用标准库实现。
- 期望能力: memory_util 与 string_util 可独立切换，不互相绑定。
- issue补充说明（评论）:
   - 在新的包管理形式中，默认应等价于 `memory_util = "std", string_util = "std"`。
   - 允许在 `em.add_libs(target, "em_base", opts)` 中分别配置：
      - `memory_util = "std" | "custom"`
      - `string_util = "std" | "custom"`

## 初步需求拆解
1. 增加 memory_util 开关控制。
2. 保持 string_util 现有开关能力，并与 memory_util 形成独立配置项。
3. 在构建配置入口（如 em.add_libs 的 options）支持:
   - memory_util = "std" | "custom"
   - string_util = "std" | "custom"
4. 默认配置等价于:
   - memory_util = "std"
   - string_util = "std"
5. 宏层面预期:
   - USE_CUSTOM_MEMORY_UTIL_IMPL 默认 0（未定义时）
   - USE_CUSTOM_STRING_UTIL_IMPL 按配置生效
6. 文档和示例需同步更新（代码即事实）。

## 代码现状确认（已完成）
1. `memory_util.h` 与 `string_util.h` 均已具备默认宏值 0（默认 std 实现）。
2. `memory_util.c` 与 `string_util.c` 通过对应宏编译 custom 实现。
3. `unittests/em_base/xmake.lua` 已存在 std/custom 分离测试目标。
4. 当前差距在构建入口：`xmake/modules/libca/em_modules/em_base.lua` 尚未读取 `opts.memory_util` / `opts.string_util` 并注入编译宏。

## 差距与改造目标
1. 为 `em_base` 模块处理器增加选项解析：
   - `memory_util` 未传时默认为 `std`
   - `string_util` 未传时默认为 `std`
2. 将选项映射为 defines：
   - `memory_util = "custom"` -> `USE_CUSTOM_MEMORY_UTIL_IMPL=1`
   - `string_util = "custom"` -> `USE_CUSTOM_STRING_UTIL_IMPL=1`
   - `std` 走头文件默认值（0），不额外注入 define
3. 对非法选项值给出 warning，并回退到 `std`。
4. 同步更新示例与文档，明确默认行为与独立切换能力。

## 待确认问题（交互中）
1. [已确认] 兼容旧调用方式：`em.add_libs(target, "em_base")` 仍可用，默认 std/std。
2. [已确认] 非法选项值处理：回退到 std，并给 warning。
3. [已确认] 测试覆盖：补齐 memory/string 四种组合验证。
4. [已确认] 文档范围：
   - 更新 em_base 设计文档，补充两个宏说明。
   - 具体使用方式放在使用文档说明。

## 开发计划
1. [已完成] 代码现状扫描：定位 em_base 中 memory/string util 宏和实现切换入口。
2. [已完成] 配置链梳理：确认 em.add_libs -> 模块编译宏传递路径。
3. [已完成] 需求确认：已确认兼容策略、错误处理、测试覆盖与文档范围。
4. [已完成] 实现改造：补齐 em_base 选项解析与 define 注入，确保 memory/string 独立。
5. [已完成] 验证：补齐并执行 memory/string 四种组合验证。
6. [已完成] 文档更新：
    - em_base 设计文档补充宏说明。
    - 使用文档补充 em.add_libs 配置示例与默认行为说明。
7. [已完成] 任务收口：本轮需求已闭环，临时文件保留用于后续追踪。

## 交互确认记录
- 2026-04-01:
   - 兼容默认行为: 保持兼容并默认 std/std。
   - 非法值策略: 回退到 std 并 warning。
   - 测试范围: 需要补齐 4 组合。
   - 文档范围: 设计文档补宏说明，使用文档写具体用法。

## 风险提示
- 若宏默认值与构建层默认值不一致，可能导致行为分叉。
- 若只改构建选项不改头文件默认宏，外部独立编译单元可能行为不一致。

## 本轮实现结果
1. `xmake/modules/libca/em_modules/em_base.lua`
   - 支持 `memory_util`/`string_util` 选项解析（`std`/`custom`）。
   - 非法值或类型不匹配时 warning 并回退 `std`。
   - `custom` 模式注入对应宏 `=1`，`std` 使用头文件默认值。
2. `libca.em/unittests/em_base/xmake.lua`
   - 新增四个组合测试目标：
     - `test-base_impl_std_std`
     - `test-base_impl_std_custom`
     - `test-base_impl_custom_std`
     - `test-base_impl_custom_custom`
3. 文档同步：
   - `libca.em/src/em_base/em_base使用文档.md` 增加选项配置、默认行为、示例。
   - `libca.em/src/em_base/em_base设计文档.md` 增加宏与 `em.add_libs` 选项映射说明。

## 验证记录
- 执行命令：逐个 build/run 四个组合测试目标。
- 结果：4/4 目标构建并运行通过，每个目标 `28 passed, 0 failed`。

## 后续追加任务（同 issue）
1. [已完成] 创建 issue 分支并携带当前改动。
2. [已完成] 使用 commit skill 生成提交信息并完成第一笔提交推送。
3. [已完成] 增加源码包管理模式的“用户版”测试（std/std 与 custom/custom）。
4. [已完成] 执行用户版测试并验证通过。
5. [已完成] 第二笔提交与推送（用户版测试）。
6. [进行中] 与用户交互验收确认，验收后才视为任务完成。
7. [已完成] 将临时文件迁移到 `archieve/issue-81/` 并纳入 git 管理。
8. [已完成] 使用 gh CLI 创建 PR，并按 personification 风格撰写内容。
9. [进行中] 用户最终验收确认。
10. [已完成] 按用户指定修复 PR review 中 `opts` 非 table 类型导致的潜在运行时错误。

## 分支与提交记录
- 分支: `feat/issue-81-em-base-impl-switch`
- 已推送提交 1: `fc8acf8`（em_base 选项切换能力、四组合矩阵测试、文档更新）
- 已推送提交 2: `3fafcfb`（用户模式源码包接入测试，覆盖 std/std 与 custom/custom）
- 已推送提交 3: `269f675`（归档 issue 过程文档到 `archieve/issue-81/`）

## PR 记录
- PR: `https://github.com/luiox/libca/pull/104`
- base/head: `main` <- `feat/issue-81-em-base-impl-switch`

## 代码审计定向修复
- 审计来源: PR #104 review（gemini-code-assist）
- 用户指定只处理项:
   - `opts = opts or {}` 对非 table 输入不安全，可能触发 `opts.memory_util` 索引错误。
- 已完成修复:
   - `xmake/modules/libca/em_modules/em_base.lua`
   - 将 `opts` 归一化为 `type(opts) == "table" and opts or {}`。
   - 仅修复该项，其它 review 建议按用户要求不处理。
- 修复后验证:
   - `test-base_user_mode_std_std` 通过
   - `test-base_user_mode_custom_custom` 通过

## 验收补充要求
- 2026-04-01 用户补充：
   - `task_plan.md` 与 `task_prompt.md` 必须加入 git 管理。
   - 两文件移动到 `archieve/issue-81/` 目录。
   - 继续使用 gh CLI 创建对应 PR。
