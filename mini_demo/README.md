# mini_demo

> 状态：历史复现材料（仅用于作用域/导出机制行为对照），不作为当前推荐接入方式。

> 当前推荐：统一使用 add_moduledirs + import("libca.em") + em.setup/em.add_libs。

本目录用于整理并复现以下问题与需求：

- 需求：把源码库封装成“一键注入当前 target”的方式。
- 约束：不希望先编译成 .o/.a 再 add_deps，避免与最终 target 编译选项不一致。
- 痛点：多层 includes + rule + os.scriptdir() 时路径语义容易混乱，维护成本高。
- 目标：找到稳定可预测的封装方式，并形成可提交 issue 的证据链。

## 背景与核心诉求

在嵌入式/交叉编译项目里，源码注入需要和当前 target 完全同构：

- toolchain
- plat/arch
- defines/cflags
- include 搜索路径

因此更偏向在当前 target 内直接注入源码，而不是先产出中间库再链接。

## 探索矩阵

本目录目前包含 6 个可运行案例：

1. `repro_nil`：复现 includes 函数在 on_load 场景下不可见（nil）
2. `repro_rule`：rule 注入对照组（可用）
3. `repro_function`：文档推荐的描述域函数封装（可用）
4. `repro_import`：import 模块 + on_load（可用）
5. `repro_doc_includes`：文档示例 includes + 描述域函数调用（可用）
6. `repro_doc_import_moduledirs`：文档示例 add_moduledirs + import + on_load（可用）

## 运行命令

### A. 失败复现：repro_nil

```bash
xmake -P mini_demo/repro_nil -vD
```

预期结果：报错 `attempt to call a nil value (global 'm_add_libs')`。

### B. rule 对照：repro_rule

```bash
xmake -P mini_demo/repro_rule -r
xmake -P mini_demo/repro_rule
```

预期结果：构建成功。

### C. 描述域函数封装：repro_function

```bash
xmake -P mini_demo/repro_function -r
xmake -P mini_demo/repro_function
```

预期结果：构建成功。

### D. import 模块方案：repro_import

```bash
xmake -P mini_demo/repro_import -r
xmake -P mini_demo/repro_import
```

预期结果：构建成功，并打印注入日志。

### E. 文档示例1：repro_doc_includes

```bash
xmake -P mini_demo/repro_doc_includes -r
xmake -P mini_demo/repro_doc_includes
```

预期结果：构建成功。

### F. 文档示例2：repro_doc_import_moduledirs

```bash
xmake -P mini_demo/repro_doc_import_moduledirs -r
xmake -P mini_demo/repro_doc_import_moduledirs
```

预期结果：构建成功。

## 关键结论

1. xmake 并不禁止函数封装 add_files/add_defines。
2. 文档推荐的“描述域函数封装”是可行的。
3. add_moduledirs + import + on_load 也是可行路径。
4. 真正的问题边界是：includes 导入函数在某些 on_load 作用域链中会出现可见性不一致（nil）。
5. 因此本问题更像“作用域语义/导出机制可预测性”问题，而不是“是否支持函数封装”问题。

## 对工程实践的影响

当前可工作方案虽多，但在复杂工程中仍有维护痛点：

- 多层 includes + scriptdir 推导路径容易偏移。
- 用户不易判断某个脚本域是否能稳定访问导入函数。
- 接口可维护性与可预测性下降，不利于对外提供统一接入 API。

## 需求抽象（提交 issue 用）

希望 xmake 提供更明确的模块导出语义，以便稳定封装“源码级注入接口”。

目标能力：

1. 在描述域与脚本域有一致的函数可见性语义。
2. 显式导出/导入机制（避免依赖隐式全局污染）。
3. 路径解析与调用上下文解耦，降低 scriptdir/includes 层级耦合。

## 建议 issue 标题

`includes-loaded function is not visible in outer script scope (nil in target on_load)`

## 候选方案（可接受）

1. 增加显式导出/导入机制（推荐）。
2. 增强 includes 作用域继承策略并文档化。
3. 提供官方配置模块 API（module/export/use 风格）。
4. 若短期不改行为，至少明确文档中各作用域可见性边界。

## 其他附加信息

- 可附带 `mini_demo.zip` 作为 issue 附件。
- 该目录已包含失败复现 + 成功对照 + 文档对照，便于维护者快速定位行为边界。
