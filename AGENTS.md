# AGENTS.md

本仓库是一个库项目，分两个部分一个是libca，提供cpp的基础设施，另外是libca.em提供嵌入式MCU的基础组件设施。

## 编码规则来源
- em 系列 (C): 必须遵守 `prompt/em_code_rule.md`
- core 系列 (C++): 遵守 `prompt/code_rule.md`
- 规则优先级: `prompt/` > 用户指令 > `doc/`

## 禁止事项 (em 系列)
- 禁止 C++ 语法和 `#pragma once`
- 禁止裸 `int`/`long`/`size_t`，必须用 `datatype.h` 定长类型 (u8/i32/usize)
- 禁止 `#include "em_xxx/yyy.h"`，跨模块必须用 `#include <em_xxx/yyy.h>`
- 禁止在 .c 文件内重复 .h 的 Doxygen 注释
- 禁止非 API 注释使用 `///`（包括文件说明、章节分隔、实现细节、测试、内部函数等）

## 测试约定
- libca.em: 测试写在源文件末尾 `#if TEST_ENABLE` 内，断言用 `TEST_EXPECT_EQ_U32` 等
- libca: Google Test，单独 `*_test.cpp` 文件

## Xmake 模块系统 (AI 易踩坑)
```lua
-- 外部集成必须用 import 方式，em.setup() 必须指定 root
local em = import("libca.em")
em.setup(target, { root = "path/to/libca" })
em.add_libs(target, {
    em_util = {},
    em_base = {}
})  -- 必须显式列出所有依赖，表内顺序无关
```
- em_driver 通过 `.lua` 描述文件声明 port_config 模式（`em_driver/<name>/<name>.lua`），不是直接加 src

## 命令速查
| 操作 | 命令 |
|------|------|
| 构建 | `xmake` |
| 配置 | `xmake f -y` |
| 运行测试 | `xmake run test-<模块名>` |
| 仅 em | `xmake f -y --with_core=n` |
| 仅 core | `xmake f -y --with_em=n` |
