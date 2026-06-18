# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Repository shape

This is a library monorepo built with **xmake**, split into two independently-shipped halves plus a legacy tree:

- **`libca/`** — modern C++17 desktop infrastructure. Per-module layout `libca/<mod>/src/libca/<mod>/*.hpp|cpp` with Google Test in `libca/<mod>/unittest/*_test.cpp`. Modules: `core`, `crypto`, `fs`, `str`, `time`, `collection` (active; wired in `libca/xmake.lua`), plus `opt`, `reflect`, `log`, `utility`, `zip` (present but not yet included). This is the half under active refactor.
- **`libca.em/`** — embedded **C99** components for MCUs (`libca.em/src/em_*`). Driver/bus/protocol/shell/crypto code with on-MCU constraints. See `libca.em/README.md` for the module + driver catalog.
- **`libca.core/`** — older C++ desktop code (`base`, `network`, `database`, `event`, `io`, `thread`, `platform/win`, `old/`). Treat as legacy; prefer adding new C++ work under `libca/`.

The two code styles are governed by **different, authoritative rule files** — read the relevant one before writing code (see Coding rules below).

## Build & test commands

Configuration is global via xmake options (`with_core`, `with_em`, `with_demo`, `with_tests`):

```bash
xmake f -y                              # configure (defaults: core+em+demo on, tests OFF)
xmake f -y --with_tests=y               # configure WITH C++ gtest targets (libca/libca.core)
xmake f -y --with_em=n                  # C++ only
xmake f -y --with_core=n                # embedded only
xmake                                   # build all enabled targets
```

Tests are grouped; the CI splits them by half:

```bash
xmake test -g core/test                 # run all C++ tests (matches Core CI)
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
  - Doxygen on every public API in the `.h`; do **not** re-copy it onto the `.c` definition. `static` internal funcs get Doxygen at the definition.
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
em.add_libs(target, "em_base", "em_util")       -- must list every dependency explicitly
```

`em_driver` components are declared via a per-driver `.lua` descriptor (`em_driver/<name>/<name>.lua`) that defines a `port_config` pattern, rather than being added as plain sources. Module/driver specs are registered through `em_registry.lua` / `register_module` / `register_driver`.

## Git & PRs

Push to a feature branch (never `main` directly) and open a PR. Recent history shows scoped commit prefixes: `[libca]`, `[global]`, etc. — match that convention.
