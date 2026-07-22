# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Repository shape

This is a library monorepo built with **xmake**, split into two independently-shipped halves:

- **`libca/`** — modern C++17 desktop infrastructure. Per-module layout `libca/<mod>/src/libca/<mod>/*.hpp|cpp` with Google Test in `libca/<mod>/unittest/*_test.cpp`. This is the half under active refactor. **Module inventory below.**
- **`libca.em/`** — embedded **C99** components for MCUs (`libca.em/src/em_*`). Driver/bus/protocol/shell/crypto code with on-MCU constraints. See `libca.em/README.md` for the module + driver catalog.

The two code styles are governed by **different, authoritative rule files** — read the relevant one before writing code (see Coding rules below).

### libca module inventory (the "is there already a wheel?" index)

Check this table before building anything new under `libca/`. **API usage lives in the header Doxygen comments** (`///`) — read the header to learn how to use a type. `doc/libca功能索引.md` is only a navigation/index document. Per-module design docs in `libca/<mod>/doc/` explain design ideas, type organization, and tradeoffs; they should not duplicate full API lists or promise compatibility.

| 模块 | 能力一句话 | 关键类型/入口 | 命名空间 | 状态 | 详情文档 |
|------|-----------|--------------|----------|------|----------|
| **core** | Result/字节/类型转换/基础类型地基 | `Result<T,E>`, `Bytes`, `cast`, `any`, `i32/u8/usize` | `ca` / `ca::core` | 主线 | `libca/core/doc/` |
| **str** | UTF-8 字符串与所有权类型 | `Utf8String`, `Utf8StringRef`, `Utf8StringArena` | `ca::str` | 主线 | `libca/str/doc/` |
| **fs** | 文件/路径操作(封装 std::filesystem) | `FileUtil`, `PathUtil`, `FileMode` | `ca::fs` | 主线 | `libca/fs/doc/` |
| **crypto** | 哈希/CRC/base64 | `sha256`, `md5`, `sha1`, `crc`, `base64` | `ca::crypto` | 可用(缺文档/测试薄) | — |
| **time** | 日期时间 | `DateTime` | `ca::time` | 可用(薄) | — |
| **collection** | 不可变列表/流 | `immutable_list`, `stream` | `ca` | 雏形(薄,杠杆高) | — |
| **ui** | Win32 GUI（窗口/按钮/消息框/防截屏），Windows-only | `Window`, `Button`, `MessageBox` | `ca::ui` | 雏形 | `libca/ui/doc/` |
| opt / reflect | — | — | — | **空,未开始** | — |
| log / utility | 有码但**未接入构建** | — | — | 暂勿依赖 | — |

`core / crypto / fs / str / time / collection / thread / io / net / http / process / ini / json / csv / toml / ui` 已接进 `libca/xmake.lua`。新增 C++ 工作放在 `libca/` 下,遵守依赖层级:**core(L0) ← str/collection(L1) ← fs/time/crypto(L2) ← 业务**,禁止向上依赖、禁止同层循环依赖。

Compatibility policy: libca does not maintain separate API freeze lists and does not promise strict long-term API/ABI compatibility. Prefer smooth source evolution, but breaking changes are allowed when they improve ownership semantics, error models, or module boundaries. Document meaningful breakage in README, CHANGELOG, or the relevant module docs.

## Build & test commands

Configuration is global via xmake options (`with_core`, `with_em`, `with_demo`, `with_tests`):

```bash
xmake f -y                              # configure (defaults: core+em+demo on, tests OFF)
xmake f -y --with_tests=y               # configure WITH C++ gtest targets (libca)
xmake f -y --with_em=n                  # C++ only
xmake f -y --with_core=n                # embedded only
xmake                                   # build all enabled targets
```

Tests are grouped; the CI splits them by half:

```bash
xmake test -g libs/test                 # run all C++ tests (matches Core CI)
xmake test -g em/test                   # run all embedded C tests (matches EM CI)
xmake test -g em/test -v                # verbose
```

Running a single test target:

```bash
xmake run libca_str_unittest            # one C++ module's gtest binary (needs --with_tests=y)
xmake run test-datatype                 # one em test target (binary; also `xmake test test-datatype`)
xmake run -d <target>                   # run under debugger
```

CI uses `--toolchain=gcc` on Ubuntu. On Windows MSVC, `/utf-8` is applied automatically. `compile_commands.json` auto-regenerates at the repo root on build.

**`with_tests` gotcha:** C++ `*_unittest` targets and the `gtest` dependency only exist when `--with_tests=y`. Default is OFF so the libs can be consumed as a submodule without forcing a gtest pull. The embedded `test-*` targets are unaffected (they use the in-tree `em_test` framework, not gtest).

## Coding rules (authoritative, by tree)

Rule precedence: **`prompt/` > user instructions > `doc/`**. The `prompt/*_code_rule.md` files override anything else.

- **em series (C)** — `prompt/em_code_rule.md`. Hard constraints AI commonly violates:
  - No C++ syntax, no `#pragma once` (use `PROJECT_PATH_FILE_H` guards). Public headers wrap declarations in `extern "C"`.
  - **No bare `int`/`long`/`size_t`/`uintptr_t`** — use `datatype.h` fixed-width types (`u8/i32/usize/f32/...`). `char` only for characters/strings. Index/length/capacity → `usize`; signed diffs → `i32`. Avoid `i64` (some targets lack it).
  - Cross-module includes use angle brackets `#include <em_xxx/yyy.h>`; same-module uses quotes `#include "yyy.h"`. Never `#include "em_xxx/yyy.h"`.
  - `self`-pointer / driver hot-path null checks use the contract macro `param_check` (from `em_base/debug.h`), not `if`. User-facing/non-hot-path checks use `if` + error code.
  - Doxygen on every public API in the header, always using `/// ` lines; do **not** re-copy it onto the source definition. Internal/file/implementation comments use ordinary `//` or `/* */`, never `///`.
- **non-em series (C++)** — `prompt/code_rule.md`. C++17 in `.cpp/.hpp`; C99 in `.c/.h` with `extern "C"` wrappers for C-callable interfaces.
- **Naming (whole repo, per `memory`):** functions/vars `snake_case`, types `CamelCase`, constants `UPPER_SNAKE`, globals `g_` prefix, em macros `CA_`-prefixed `UPPER_SNAKE` (user-facing function-like macros may be `snake_case`). Style: 4-space indent, K&R braces, 120-col, Chinese comments (don't translate English technical terms). `.clang-format` is the formatter; the rule file wins on conflict.

## Embedded testing model (libca.em)

Unlike the C++ half, em tests are **inline in the source file**, gated by `#if TEST_ENABLE`, using the `<em_test/test.h>` framework (`TEST_CASE`, `TEST_EXPECT_EQ_U32`, etc. — full assertion list in `libca.em/README.md`). Test targets live in `libca.em/unittests/<module>/xmake.lua`, named `test-<sourcename>`, and opt in via:

```lua
add_rules("em_test", { test_enable = true, use_default_main = true })
```

This rule (defined in `libca.em/src/em_test/xmake.lua`) injects the framework sources + `main`, defines `TEST_ENABLE=1`, sets the target's group to `em/test`, and registers it with `xmake test`. Tests must mock/run on Windows + Linux using `em_test`'s test-double support.

## xmake module system for embedded (external integration)

When another project consumes `libca.em`, it uses the source-package importer (entry: `xmake/modules/libca/em.lua`), **not** raw `add_files`:

```lua
local em = import("libca.em")
em.setup(target, { root = "path/to/libca" })   -- root is required
em.add_libs(target, {
    em_util = {},
    em_base = {}
})                                               -- explicit dependencies; order does not matter
```

`add_libs` accepts either the canonical module table above or the compatible single-module form
`em.add_libs(target, "em_base", opts)`. All calls made in `on_load` are validated and applied together in dependency
order, so call order is irrelevant and a repeated module keeps its latest options. Dependencies are never added
implicitly.

`em_driver` components are declared via a per-driver `.lua` descriptor (`em_driver/<name>/<name>.lua`) that defines
static `src`, optional `deps`, and a `port_config` pattern, rather than being added as plain sources. Module/driver specs
are registered through `em_registry.lua` / `register_module` / `register_driver`. `em_eimui` remains a host-side SDL
prototype and is intentionally absent from the MCU source-package catalog.

## Git & PRs

Push to a feature branch (never `main` directly) and open a PR. Recent history shows scoped commit prefixes: `[libca]`, `[global]`, etc. — match that convention.
