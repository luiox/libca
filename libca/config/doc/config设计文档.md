---
version: 1.0
update:
2026-09-13 - 首版：记录 config 模块的关键设计取舍
---

# libca_config 设计文档

## 1. 目标与边界

`libca_config` 是 sylar 风格配置中心的 libca 适配：进程级静态注册中心 + 强类型
配置项 `ConfigVar<T>` + 变更监听器，配置载体为 JSON。接口形态上对齐 sylar 的
`Config::Lookup<T>` 语义（按名查找、不存在即建、类型冲突报错），实现完全按 libca
的风格重写（snake_case、Result 错误通道、中文注释）。

依赖方向（单向向下，符合分层约束）：

```text
libca_core <- libca_str <- libca_json <- libca_fs
                                    ^     ^
                                     \   /
                                   libca_config
```

API 事实来源是头文件 Doxygen 注释，本文只记录设计取舍。

## 2. 为什么用 JSON 而非 YAML

仓库已有完整的 JSON 模块（解析器 + DOM + 序列化器，含 arena 字符串池），而
YAML 是超集级语法，自写解析器成本高且无现成依赖。配置格式选 JSON 直接复用
`JsonReader::read → JsonDocument`，错误通道走 `ParseError`，往返序列化走
`JsonWriter`，不引入新解析代码与新第三方依赖。代价是牺牲 YAML 的锚点/多行字面量
等语法糖，对配置中心场景可接受。

## 3. 两级锁与锁外回调

并发模型是两级锁：

- **注册表锁**（`ConfigState::mutex`，`std::shared_mutex`）：保护 vars / pending
  两张表。lookup 注册、load 装配应用清单、visit 取快照用独占锁；lookup_base、
  visit 快照用共享锁。
- **配置项锁**（每个 `ConfigVar<T>` 内部 `std::shared_mutex`）：保护值、监听器表
  与 to_json scratch arena。读值用共享锁，set/add/remove 用独占锁。

监听器回调在**两级锁之外**触发：`set()` 先在独占锁内换值并把监听器列表拷出，
释放锁后逐个调用。这是刻意设计——回调中常见"再读其它配置项"（进注册表锁）甚至
"写本配置项"，持锁回调会自锁死。代价有两个，均为可接受并已文档化：

- 监听器触发顺序 = 注册顺序，但多监听器之间没有事务性；
- `set` 返回时回调可能尚未跑完（并发场景下观测者与写入者解耦）。

同理，`Config::load` 把"装配应用清单"（持注册表锁）与"逐项应用"（锁外调用
`set_from_json`）拆成两段。代价是两个并发 load 的应用顺序不保证（最后写入者胜，
且以 var 锁内的单次 `operator==` 比较为准，不会出现撕裂值）；收益是 load 期间
监听器/lookup 不被长时间阻塞，且没有死锁路径（锁序恒为"注册表锁 → 配置项锁"，
后者内永不取前者）。

## 4. 未物化 key 的物化语义

load 遇到尚无人认领的 key 时，不做类型猜测，而是把 key 连同**整份解析文档**
（`shared_ptr<JsonDocument>`，保活字符串 arena）存入 pending 表。之后
`lookup<T>(name, ...)` 首次命中该 key 时物化：

- `JsonCast<T>` 转换成功 → 以加载值为初值；
- 转换失败 / 类型不符 → **退回本次 lookup 传入的 default**，注册照常成功。

"类型不符退回 default 而不是报错"是取舍：配置文件先于代码加载时，出现代码从未
注册过的 key 是常态而非错误；等代码真正需要这个 key 时才做类型仲裁，此时调用方
恰好手上有合理的 default。副作用是类型错误被静默吞掉，故 visit 会把 pending 条目
以其 JSON 文本原样呈现（`pending=true`），诊断时能看到原始值。

物化只发生一次（命中即从 pending 表删除），之后与普通注册项无异：类型冲突返回
nullptr，重复 lookup 幂等返回同实例。

## 5. load 的失败语义：整体拒绝 vs 部分生效

分两级，错误类型统一为 `Result<void, ConfigErrorInfo>`：

- **整体拒绝（状态零变化）**：非法 JSON、顶层非 object。解析阶段不持任何锁，
  失败直接返回，注册表与 pending 表零改动。
- **部分生效（仍返回 Err）**：单 key 与已注册 var 类型不匹配/超范围时跳过该 key，
  其余 key 照常生效，最后返回 `Err(TYPE_MISMATCH)`，`keys` 列出被跳过的 key。

部分生效仍返回 Err 而不是 Ok 携带警告，是因为调用方需要显式感知"这次配置没有
完整生效"——把信息塞进成功通道会诱导调用方不看。生成了哪些 key 可从 keys 的
补集推知，测试里对 message/keys 都有断言。

## 6. 其它取舍

- **to_json 的字符串生命周期**：`JsonValue` 的字符串是指向 arena 的视图。模块提供
  两个形态：`to_json()`（便捷，字符串 intern 到 var 内部 scratch arena，仅到该 var
  下一次 to_json 前有效）与 `to_json(JsonDocument&)`（显式生命周期，visit 内部用
  这个）。并发或长持有场景必须用后者。
- **取值返回拷贝**：`value()` 返回 `T` 拷贝而非引用，换内部锁保护下的完整快照，
  避免引用在并发 set 下悬空/撕裂。配置值都是小对象，拷贝成本可忽略。
- **整型范围**：JSON Int 以 i64 存储，`JsonCast` 对窄整型做范围检查（OUT_OF_RANGE）；
  u64 高位超出 i64 正域的值在 JSON 解析期已降级为 Float，对本转换表现为类型不符
  ——这是 JSON 表示能力的固有限制，非本模块 bug。
- **id 从 1 开始**：监听器 id 0 保留为无效值，`add_listener` 首个返回 1，
  `remove_listener(0)` 恒 false。
- **`Config::clear()`**：注册中心是进程级单例，为测试隔离与程序初始化阶段重建
  提供清空入口；已逃逸的 `shared_ptr` 不受影响。

## 7. 测试组织

单测 25 例覆盖：lookup 幂等/类型冲突/默认值、set 短路与监听器旧新值、load 应用
与监听、未知 key 物化与 default 退回、嵌套容器（vector/int/string、map 嵌套
vector）、非法 JSON 整体拒绝、单 key 跳过并报详情、load_file 往返（临时文件）、
visit 覆盖 pending、并发 smoke（2 lookup + 1 load + 原子计数监听器，5 秒总截止，
线程内自检 deadline，无无界等待）。
