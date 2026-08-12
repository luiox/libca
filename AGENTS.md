# AGENTS.md

本仓库是一个库项目（xmake 构建），分两个相对独立的部分：

- `libca/`：C++17 桌面端基础设施（Rust 语义对齐的现代 C++ 标准库补充）
- `libca.em/`：C99 嵌入式 MCU 组件（`em_` 系列：驱动/总线/协议/shell 等）

## 规则优先级与编码规范

规则优先级：**`prompt/` > 用户指令 > `doc/`**。

- em 系列 (C)：必须遵守 `prompt/em_code_rule.md`（唯一权威）
- libca 系列 (C++)：遵守 `prompt/code_rule.md`（唯一权威）
- 命名/风格（全库）：函数/变量 `snake_case`、类型 `CamelCase`、常量 `UPPER_SNAKE`、全局变量 `g_` 前缀；4 空格缩进、K&R 花括号、单行最长 120；注释用中文（英文技术术语不翻译）。`.clang-format` 是格式化工具，与规则文件冲突时以规则文件为准。

## 仓库结构

- libca 模块布局：`libca/<mod>/src/libca/<mod>/*.hpp|cpp` + `unittest/*_test.cpp`（Google Test）+ `doc/`（设计文档，每模块最多一个）
- libca.em 模块布局：`libca.em/src/em_xxx/`；测试 target 在 `libca.em/unittests/<mod>/xmake.lua`
- 头文件包含：C++ 用 `#include <libca/<mod>/xxx.hpp>`；em 跨模块用 `#include <em_xxx/yyy.h>`、同模块用 `"yyy.h"`

## C++ 依赖分层（libca）

单向依赖，禁止向上依赖、禁止同层循环：

```
L0  core              ← 地基，不依赖任何 libca 模块
L1  str, collection   ← 仅依赖 core
L2  fs, time, crypto  ← 依赖 L0/L1
L3  业务 / 上层
```

改 L0 会连锁影响下游，需谨慎。模块清单见 `README.md` 的「模块一览」和 `doc/libca功能索引.md`。

## 模块接入构建

- 新增 libca 模块：建 `libca/<mod>/` 后必须加入 `libca/xmake.lua`（`includes("<mod>")`），否则不参与构建
- 新增 em 模块：加入 `libca.em/xmake.lua`；`em_driver` 通过 `.lua` 描述文件声明（`em_driver/<name>/<name>.lua`），不是直接加 src

## 测试约定

- libca (C++)：Google Test，写在 `libca/<mod>/unittest/*_test.cpp`，target 名 `libca_<mod>_unittest`。**只有 `--with_tests=y` 才会生成这些 target**（默认关，避免作为 submodule 时强拉 gtest）
- libca.em：测试写在源文件末尾 `#if TEST_ENABLE` 内，断言用 `TEST_EXPECT_EQ_U32` 等（`em_test` 框架）；测试 target 在 `libca.em/unittests/<mod>/xmake.lua`，命名 `test-<源文件名>`，用 `add_rules("em_test", { test_enable = true, use_default_main = true })` 注册

## 命令速查

| 操作 | 命令 |
|------|------|
| 配置（默认） | `xmake f -y` |
| 配置（带 C++ 测试） | `xmake f -y --with_tests=y` |
| 构建 | `xmake` |
| C++ 全量测试 | `xmake test -g libs/test` |
| em 全量测试 | `xmake test -g em/test` |
| 单个 C++ 测试 | `xmake run libca_<模块>_unittest` |
| 单个 em 测试 | `xmake run test-<源文件名>` |
| 仅 em | `xmake f -y --with_core=n` |
| 仅 libca | `xmake f -y --with_em=n` |

## Git 与 PR 工作流

- 禁止直接推 `main`；功能分支用 `codex/xxx` 或 `feat/xxx`，提交信息带范围前缀（`[libca]` / `[libca.em]` / `[global]`）
- 合入 main 多为 squash/rebase：合并后远端分支通常被删除，本地分支的提交 hash 不会出现在 main（`git cherry`/patch-id 对不上是正常的，不代表未合并）
- 清理本地分支前先用 `gh` 确认：`gh pr list --head <branch> --state merged` 或 `gh pr view <PR>`。PR 已合并、或远端分支仍存在（可恢复）才删除：`git branch -D <branch>`
- 判断 squash 后是否真的合入，看内容：对应类/文件是否在 main、`git diff origin/main <branch>` 是否为空或只增不减

## Xmake 模块系统（em 外部集成，AI 易踩坑）

```lua
-- 外部集成必须用 import 方式，em.setup() 必须指定 root
local em = import("libca.em")
em.setup(target, { root = "path/to/libca" })
em.add_libs(target, {
    em_util = {},
    em_base = {}
})  -- 必须显式列出所有依赖，表内顺序无关
```

- `add_libs` 也支持单模块形式：`em.add_libs(target, "em_base", opts)`

## 禁止事项（em 系列）

- 禁止 C++ 语法和 `#pragma once`（头文件保护用 `项目名_路径_文件名_H`）
- 禁止裸 `int`/`long`/`size_t`/`uintptr_t`，必须用 `datatype.h` 定长类型（u8/i32/usize/f32…）；索引/长度/容量用 `usize`，可能为负的差值用 `i32`，少用 `i64`
- 禁止 `#include "em_xxx/yyy.h"`，跨模块必须用 `#include <em_xxx/yyy.h>`
- 禁止在 .c 文件内重复 .h 的 Doxygen 注释
- 禁止非 API 注释使用 `///`（文件说明、章节分隔、实现细节、测试、内部函数一律用 `//` 或 `/* */`）
- 驱动 `self` 指针等热路径空指针检查用 `em_base/debug.h` 的 `param_check` 契约宏，不用 `if`

## 文档维护

- 头文件 Doxygen 注释是 API 使用方式的唯一事实来源；`doc/` 只记录设计与取舍，不维护接口清单
- 不兼容变更需在 README / CHANGELOG / 模块文档中通知下游
