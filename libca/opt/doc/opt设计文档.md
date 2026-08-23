---
version: 1.2
update:
2026-08-23 - 新增 OptionalString 可选值形态（--x 裸出现 / --x=v 内联，
             空格形态不消费后继 token）
2026-08-23 - 新增值来源查询 source_of()；互斥组判定改为显式选择（静态默认不算）；
             写明 Int 十进制 i32 边界（评审反馈）
2026-08-22 - 初版：v2 重写落地（P0-P2），记录三套 CLI 实现收敛的背景与关键取舍
---

# libca::opt 设计文档

> 本文记录 opt 模块的架构、边界、取舍。**不含接口签名与方法说明（见头文件 Doxygen 注释）**。
> 涉及接口见 `libca/opt/src/libca/opt/opt.hpp`。

## 1. 模块定位

命令行选项解析器，位于业务上层，仅依赖 core（`Result`/`StatusCode`）与 str（`format_std`）。

目标：成为下游项目（morpher mjt、mj2x-cli 等）的共同 CLI 地基，替代 mbase::ArgParser
与 mj2x-cli param_registry 中各自为政的解析逻辑。背景是仓库外曾并行存在三套 CLI 实现，
能力互有长短、维护成本翻倍，收敛动机与差距分析见提案 `doc/proposals/opt-v2.md`。

边界（不进库，由调用方在库外组装）：

- 配置文件格式与解析（下游用 libca/json 等自行加载后经初值注入接入）
- schema / template dump（下游基于 Arg 元数据自建，供 GUI 等消费）
- 错误文案与 i18n（库只产出「类别 + 出错参数名」，message 仅为现成英文兜底）

库为此提供的钩子：带初始值注入的 `parse()` 重载、`Parser::root()` 只读元数据访问、
`help_text(cmd, groups)` 分组过滤帮助渲染。

## 2. 核心模型

### 2.1 Command 树

根命令 + 任意深度子命令，每层独立持有选项表（args）、互斥组（mutex_groups）、
usage 与 help。解析按非选项 token 逐层分派；**进入子命令前先对父命令做收尾校验**
（required 与互斥组），避免"根级约束被子命令分派跳过"这类回归。任何层级都响应
`--help` / `-h`，输出当前层的帮助文本。

### 2.2 Arg 三要素

- **name**：canonical key，同时是存储 key——`has()/get()/get_int()/get_list()` 只认它；
  即便选项只用短名也必须提供长名作 key。
- **aliases**：额外的命令行 token（含 `-` 前缀完整书写）。别名只影响匹配与 help 展示，
  不影响取值入口。
- **kind**：取值类型的唯一表达，不存在"有值但类型未定"的中间态。

各 kind 的重复语义：

| kind | CLI 形态 | 重复出现 | 初值注入 |
|------|----------|----------|----------|
| Flag | `-v` / `--verbose` | 幂等置位 | 不参与 |
| String | `--out x` / `--out=x` / `-o x` / `-ox` | last-wins | 覆盖默认值 |
| Int | 同上 | last-wins，非法即报错 | 注入时即校验 |
| StringList | `--p a,b,c` 或多次出现 | 追加合并 | 逗号拆分追加 |
| OptionalString | `--dump`（裸） / `--dump=x`（内联）/ `-d` / `-dx` | last-wins | 覆盖默认值 |
| Positional | 裸 token | 按序收集 | 不参与 |

未声明 Positional 时多余裸 token 报 UnexpectedArgument（旧版静默丢弃属缺陷，v2 收紧）；
`--` 终止符后的 token 一律进 positionals。

**OptionalString 的歧义收敛**：可选值是经典 CLI 需求（如
`--dump-config-schema[=file]` 不带值输出 stdout、带值写文件），但
"`--x value` 算不算给值"与位置参数/子命令天然冲突。v2 采用 GNU getopt_long
optional_argument 的同款收敛：**空格形态永不消费后继 token**——值只能内联
（长选项 `=` 形态）或附着（短选项 `-dfile`）；裸出现 = 已提供且值为空串，
语义归调用方约定。配合 source_of 可精确区分"没给 / 给了空的"。

**Int 的边界**：严格十进制 `int32`（`strtoll` 全量消费 + 溢出拒绝），不支持 `0x`
前缀与无符号全值域。hex（如 `0x` 种子值）或 u64 场景走 String 选项由下游
`strtoull` 自行解析——进制与位宽是领域语义，不值得为此扩 OptKind 面。

## 3. 关键决策与取舍

### 3.1 不做兼容层

旧 `Arg{short_name, has_value}` 双字段与新 `kind + aliases` 在语义上无法无损映射
（has_value=true 的 Flag 实际是 String），保留两套表达只会让错误配置更隐蔽。
该子库当时无下游用户，直接移除旧字段，破坏性变更记入 CHANGELOG。

### 3.2 错误模型：类别优先于文案

`parse()` 返回 `Result<ParseResult, ParseError>`，错误载荷是
`ParseErrorCategory + option + message`：

- **类别是契约，文案是兜底**。调用方按 category 决定行为（映射退出码、提示重试、
  打印帮助），可整体替换文案做 i18n；不想定制的场景直接打印 message 也能用。
- **HelpRequested 走 Err 分支**：用户请求帮助会中止正常流程，与成功路径互斥；
  放 Err 里让 `is_ok()` 严格等价于"可以继续干活"，message 直接承载完整帮助文本。
  `to_status_code()` 提供到通用 StatusCode 的桥接（HelpRequested → CANCELLED），
  兼容仍以 Status 为边界的调用方。
- **InvalidDefinition 单列**：空 name、别名不带 `-`、token 重复、required 与 default
  并存、互斥组引用未注册选项等属于**定义期程序错误**，与用户输入错误（UnknownOption
  等）分开，桥接为 FAILED_PRECONDITION。fail-fast：在 parse 入口与子命令切换时即拒绝，
  不等到运行期。

### 3.3 互斥组

- 用结构体字段 `Command::mutex_groups` 表达而非 builder 方法：聚合初始化即可声明，
  库侧不引入构建器层；若未来迁移量大再加 builder 语法糖。
- 组内成员至多出现一个（冲突报 MutexConflict），required 时至少一个（缺失报
  MutexRequired）。help 中互斥成员标注 `[exclusive]` / `[exclusive, one required]`。
- **「出现」按显式选择判定**：只有命令行给出或注入初值算选择，静态 default_value
  预置不算。否则两个带默认值成员同组会在用户什么都没给时报冲突，required 组也
  会被默认值永久短路——判定依据是 `ValueSource`，不是 `has()`。
- 成员必须指向本命令已注册选项，否则 InvalidDefinition——拼错名字不应安静失效。

### 3.4 初始值注入的三级优先级与边界

`parse(argc, argv, initial_values)` 支持把配置文件加载结果批量预填，优先级恒为：

```
静态 default_value < initial_values（如配置文件） < 命令行显式出现
```

边界取舍：

- **仅带值选项参与**。Flag 无法用"presence"区分 true/false——注入 `"false"` 后
  `has()` 返回什么都是错的，干脆不允许，布尔类配置留给下游转成显式 String/Int 选项。
- **未知名字静默忽略**。配置文件通常还包含非 CLI 键（日志级别、窗口尺寸等），
  逐键报错会让注入不可用；拼写校验交给下游自己的配置 schema。
- **注入时即校验**：Int 非法、空串在 parse 入口就报 InvalidInteger / EmptyValue，
  不留到 get 阶段安静回落默认值。
- required 把注入视为已提供；互斥组按 §3.3 的「选择」语义处理（注入算选择，
  默认不算）。
- 进入每个 command 时按该命令的选项名匹配一次，子命令级选项同样可被预填。

### 3.5 值来源查询

种子值先行写入后 `has()` 无法区分「CLI 显式 / 注入 / 恰好等于默认」，对
「配置文件 + CLI」双源下游是陷阱（无法干净实现"仅当用户显式给值才告警/覆盖"
这类逻辑）。因此提供 `source_of(name)` 返回 `ValueSource { None, CommandLine,
Initial, Default }`：seed 与 CLI 写入分开登记来源，成本极低；下游的
`*Selected` 标志族模式由它等价替代。

### 3.6 help 渲染单一实现

`--help` 输出与公开的 `help_text()` 共享同一渲染函数，杜绝"CLI 帮助与库内导出"
两处漂移。渲染规则：

- 未分组选项归入 `Options:` 默认节；`Arg::group` 相同的选项按标签首次出现顺序各成一节
- `groups` 过滤参数只保留指定分组节（省略默认节），位置参数与子命令摘要不受影响——
  服务 mj2x 式分组打印与按模块裁剪帮助的需求
- 自定义 usage 完整替换首行的自动生成部分（含程序名，定义方负责书写）

### 3.7 元数据导出边界

`Parser::root()` 提供 const 访问遍历整棵 Command 树；schema / template dump /
补全脚本由下游基于 Arg 字段自建并序列化。库不定义 dump 格式：那会把 JSON/GUI
关注点拖进依赖分层。

## 4. 功能裁切

不需要专门机制。条件注册即天然 fail-closed：

```cpp
#if MJ2X_WITH_LLVM
    cmd.args.push_back(llvm_backend_arg());
#endif
```

裁掉的名字回到 UnknownOption 路径，help 由剩余选项自动生成，无需同步两处文本。
`CommandBuilder` 未实现：聚合初始化已够用，待下游真实迁移时再评估是否值得加语法糖。

## 5. 测试

`libca/opt/unittest/opt_test.cpp`，44 个用例分四组：

- OptParseTest：旧验收面（形态解析、子命令、-- 终止符、--help、基础错误）
- OptV2Test：P0 能力（positionals、Int 校验、多别名 canonical、StringList 合并、last-wins）
- OptV2P1Test：错误分类逐类断言、互斥组三态、分组渲染、自定义 usage、状态码桥接
- OptV2P2Test：三级优先级、注入校验与作用域、required/互斥组被初值满足、
  root() 元数据访问、help_text 过滤
