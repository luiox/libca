# 提案：opt v2（命令行解析器补齐）

> **状态**：已实施（2026-08，`feat/opt-v2`）。落地记录与最终取舍见
> `libca/opt/doc/opt设计文档.md`。与本文的差异点：
> `parse()` 返回 `Result<ParseResult, ParseError>` 而非保留 StatusResult 外壳；
> 互斥组为 `Command::mutex_groups` 结构体字段而非 `add_mutex_group` builder；
> 错误类别新增 InvalidDefinition（定义期错误与输入错误分离）；
> `CommandBuilder` 未实现（聚合初始化已够用）。

## 背景

当前存在三套并行的 CLI 实现，能力互有长短、维护成本翻倍：

| 实现 | 位置 | 能力 | 主要缺陷 |
|------|------|------|----------|
| `ca::opt` | libca/opt（本库） | 子命令树、`--n v`/`--n=v`/`-x v`/`-xv`/`-abc` 组合、默认值、required、`--` 终止符、任意层级 `--help` | **位置参数被静默丢弃**（opt.cpp 末尾）；值全为 string 无类型；重复选项 last-wins 无累积语义；无互斥组；错误只有 INVALID_ARGUMENT/CANCELLED 两档 |
| `mbase::ArgParser` | morpher/mbase（463 行 header-only） | 类型化 OptKind（Flag/String/Int/CommaList/**Positional**）、多别名列表、互斥组（含 required）、细粒度错误枚举、getInt/getList 带默认 | 平行轮子，被困在 mbase，其他项目复用不到；无子命令 |
| `mj2x-cli` param_registry | mj2x/mj2x-cli（920 行） | 一张 ParamDef 表驱动四个消费端：CLI 解析 + JSON 配置文件 + help 分组打印 + JSON Schema/template dump（GUI 消费）；defaults < config < CLI 三级优先 | 每参数手写 parseCli/parseJson 函数指针样板，量极大 |

## 需求来源

### mjt（D:\WorkSpace\morpher\mjt）

15 个子命令经 `CommandRegistry` 分发，每个命令内部用 mbase::ArgParser。需要：
位置参数、Int 类型化与非法整数报错、逗号列表（`-p A,B,C` 与 `-p A -p B` 等价）、
互斥组（如 `-s/--interface` 二选一且必选）、稳定错误类别映射退出码。

### mj2x-cli（D:\WorkSpace\morpher\mj2x\mj2x-cli）

**功能裁切是硬需求**：构建开关（如 `with_mj2x_llvm=n`）关闭时，相关选项组
（`--backend llvm`/`--emit-llvm`/`--llvm-tool`）必须 fail-closed 地从 CLI 消失
（未注册即 unknown option），help 输出同步收敛。
另需：一张参数表同时喂 CLI/config/help/schema 四个消费端；
三级优先级（内置默认 < 配置文件 < 命令行）。

## 目标定位

ca::opt 成为上述项目的共同地基：替代 mbase::ArgParser 的解析职责，
并为 mj2x 式「注册表驱动多消费端」架构提供足够的元数据与钩子。
配置文件解析、schema dump、i18n 文案**不进 libca**（依赖分层不允许），
但库必须让它们在库外容易搭起来。

## 差距清单与改造项

### P0（缺陷修复 + 下游阻塞项）

1. **Positional 参数收集**：当前非选项 token 若不是子命令则静默丢弃，属缺陷。
   改为收集到 `positionals()`；未声明任何 Positional 时报 UnexpectedArgument。
2. **类型化选项**：`OptKind { Flag, String, Int, StringList, Positional }`；
   Int 非法时 InvalidInteger 错误。取值按 canonical 名。
3. **多别名**：`Arg::name` 是 canonical key 与存储 key；`aliases` 列表承载附加
   token（含前缀完整书写），如 `name="input", aliases={"-i", "--in"}`。
   替代现有 short_name 单字段（迁移点）。
4. **重复出现语义**：String/Int 默认 last-wins；StringList 追加（同时接受
   `--p A,B,C` 逗号拆分与多次出现）。
5. **带默认的取值**：`get(name, default)` / `get_int(name, default)` 重载。

### P1（错误模型 + 分组）

6. **细粒度错误分类**：`ParseErrorCategory { HelpRequested, UnknownOption, MissingValue,
   EmptyValue, UnexpectedArgument, MissingRequired, InvalidInteger, MutexConflict,
   MutexRequired, InvalidDefinition }`。`parse()` 返回
   `Result<ParseResult, ParseError>`（category + 出错选项名 + 现成英文描述），
   libca 只产出「类别 + 出错参数名」，文案与 i18n 归调用方；
   原 CANCELLED+help 文本模式迁移为 HelpRequested 类别。
7. **互斥组**：`add_mutex_group(names, required)`；help 中可标注。
8. **选项分组**：Arg 增加 group 字段，help 按 group 分节渲染（服务 mj2x 的
   分组打印需求）。
9. **usage line**：Command 增加可选 usage 描述，help 首行使用。

### P2（配置桥 + 元数据导出）

10. **初始值注入**：`Parser::parse(argc, argv, initial_values)` 或等价 API，
    允许调用方把配置文件加载结果批量预填（等价于可被 CLI 覆盖的动态默认值），
    三级优先级自然成立。
11. **元数据只读视图**：遍历 Command 树的 const 访问已具备；补充按 group 过滤的
    帮助渲染入口，下游 schema/template dump 基于 Arg 元数据自建，不进库。

### 功能裁切的落地方式

不需要专门机制：builder 式条件注册即天然支持——

```cpp
ca::opt::CommandBuilder cmd("mj2x");
cmd.add_string({"--input", "-i"}, "<jar>", "...", /*required=*/true);
#if MJ2X_WITH_LLVM
cmd.add_string({"--backend"}, "<name>", "...").add_string({"--emit-llvm"}, ...);
#endif
```

裁掉后这些名字回到 UnknownOption 路径，fail-closed 自动成立；
help 由剩余选项自动生成，无需同步两处文本。

## API 兼容性

该子库目前无下游用户，不做兼容层：`Arg::short_name` / `has_value` 旧字段在 P0 中
直接移除，短名与多别名统一为 `aliases` 列表（canonical 名始终是 `name`），
取值类型唯一由 `kind` 表达。P1 起 `parse()` 返回 `Result<ParseResult, ParseError>`
（category + 出错选项名 + 现成描述），不再复用 StatusResult 外壳。
破坏性变更记入 CHANGELOG。

## 实施阶段

| 阶段 | 内容 | 验收 |
|------|------|------|
| V2-P0 | 差距清单 1-5 + 单测 | 现有 opt 测试全绿 + 新增 positionals/类型化/别名用例 |
| V2-P1 | 差距清单 6-9 + 单测 | mbase::ArgParser 现有用例可在 ca::opt 上等价表达（对照测试） |
| V2-P2 | 差距清单 10-11 | 三级优先级集成用例；mjt/mj2x 迁移评估报告 |

## 不在范围

- 配置文件格式与解析（下游用 libca/json 自行实现）
- 子命令自动补全、shell completion
- Windows/POSIX 通配符展开
