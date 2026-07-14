# libca

C/C++ 基础设施库集合。一个仓库，三个相对独立的部分：

| 部分 | 语言 | 面向 | 入口 |
|------|------|------|------|
| **`libca/`** | C++17 | 桌面端基础设施（Rust 语义对齐的现代 C++ 标准库补充） | 本文档 §libca |
| **`libca.em/`** | C99 | 嵌入式 MCU 组件（驱动/总线/协议/shell） | `libca.em/README.md` |
| **`libca.core/`** | C++ | 旧桌面代码（legacy，逐步被 `libca/` 取代） | — |

> 三者构建上由根 `xmake.lua` 的 `with_core` / `with_em` / `with_demo` 开关解耦，可单独构建。
> 本 README 的详细部分聚焦 **libca**（桌面 C++）。嵌入式见 `libca.em/README.md`。

## 构建与测试

xmake 构建。测试 target 受 `with_tests` 开关守护（默认关，便于作为 submodule 被引用时不强拉 gtest）。

```bash
xmake f -p windows -a x64 --with_tests=y --with_em=n --with_demo=n -y  # 配置(带测试,仅C++)
xmake                          # 构建
xmake test -g core/test        # 跑全部 C++ 测试(对应 CI)
xmake run libca_fs_unittest    # 跑单个模块测试
```

> Windows 上若 gtest 在 mingw 下安装失败，用 `-p windows -a x64` 走 msvc 工具链。

---

# libca（桌面 C++）

## 设计理念

- **对齐 Rust 语义**：`Result<T,E>` 替代异常、`Ok/Err`、`Utf8String` 所有权模型。
- **现代 C++17**，作为标准库的补充，不是替代。
- **API 文档写在头文件**（Doxygen 注释）——查头文件即得「怎么用」。
- **文档只做导航和设计说明**：总功能索引用来找模块；各模块设计文档只讲思想、类型组织和关键取舍，不维护接口清单。
- **编码规范**：`spec/cpp-code-spec.md`（libca C++ 唯一权威）。
- **不做严格兼容承诺**：尽量保持常用接口平滑演进；必要的不兼容改动通过 README、CHANGELOG 或模块文档通知下游。

## 依赖分层

单向依赖，禁止向上依赖、禁止同层循环：

```
L0  core              ← 地基，不依赖任何 libca 模块
L1  str, collection   ← 仅依赖 core
L2  fs, time, crypto  ← 依赖 L0/L1
L3  业务 / 上层
```

新增模块按此分层放置；改 L0 会连锁影响下游，需谨慎。

## 模块一览

遇到需求先查这张表：有没有现成的轮子、它在哪、查哪个头文件。

| 模块 | 职责 | 关键类型 / 入口头文件 | 命名空间 | 阶段 | 设计文档 |
|------|------|----------------------|----------|------|----------|
| **core** | Result/字节/类型转换/定长类型，全库地基 | `result.hpp`(`Result<T,E>`)、`bytes.hpp`、`cast.hpp`、`any.hpp`、`datatype.hpp` | `ca` / `ca::core` | 主线 | `libca/core/doc/core设计文档.md` |
| **str** | UTF-8 字符串与所有权类型族 | `utf8_string.hpp`(`Utf8String`/`Utf8StringRef`)、`utf8_string_arena.hpp`、`cstring.hpp`、`wstring.hpp` | `ca::str` | 主线 | `libca/str/doc/str设计文档.md` |
| **fs** | 文件/路径操作（封装 std::filesystem） | `file_util.hpp`(`FileUtil`)、`path_util.hpp`(`PathUtil`) | `ca::fs` | 主线 | `libca/fs/doc/fs设计文档.md` |
| **crypto** | 哈希/CRC/base64 | `hash.hpp`、`sha256.hpp`、`md5.hpp`、`sha1.hpp`、`crc.hpp`、`base64.hpp` | `ca::crypto` | 可用（测试待补） | — |
| **time** | 日期时间 | `datetime.hpp`(`DateTime`)、`duration.hpp`、`timestamp.hpp` | `ca::time` | 可用（薄） | `libca/time/doc/time设计文档.md` |
| **collection** | Rust-like 容器（ArrayList/HashMap/HashSet/不可变列表/流） | `array_list.hpp`、`hash_map.hpp`、`hash_set.hpp`、`immutable_list.hpp`、`stream.hpp` | `ca::collection` | 主线 | `libca/collection/doc/collection设计文档.md` |
| opt / reflect / zip | 规划中 | — | — | **空** | — |
| log / utility | 有代码但**未接入构建** | — | — | 暂勿依赖 | — |

> 接入构建的模块见 `libca/xmake.lua`（当前：core / crypto / fs / str / time / collection）。
> 更详细的功能导航见 `doc/libca功能索引.md`；具体 API 以对应头文件 Doxygen 注释为准。

## 目录约定

每个模块统一布局：

```
libca/<mod>/
├── doc/                       ← 设计文档（为什么这么设计）
├── src/libca/<mod>/*.hpp|cpp  ← 声明+实现；API 文档在 .hpp 注释
├── unittest/*_test.cpp        ← Google Test
└── xmake.lua
```

头文件包含路径用安装形式：`#include <libca/<mod>/xxx.hpp>`。

