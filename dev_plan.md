# 开发计划（dev_plan）

来源：2026-08-31 全库完整度盘点（libca C++ 与 libca.em 两部分）。
本文档只记录「做什么、为什么、做到哪了」，接口细节仍以头文件 Doxygen 为准。

状态标记：`[本分支]` 进行中 / `[待办]` 未开始 / `[决策]` 需要拍板 / `[搁置]` 明确不做。

## 高优先级

### 1. Stream 管道化重写 `[本分支: feat/stream-pipeline-ipc-tests]`

现状：`collection/stream.hpp` 的 filter/map 是**覆盖式**——`.filter(p1).filter(p2)`
静默只应用 p2（头文件 @warning 已声明，测试也钉死了该契约）。这与「Rust 语义对齐」
的定位冲突，且整个流只有 filter/map/for_each/collect 四个操作。

目标：

- 重写为惰性流水线：每个适配器返回新节点（按值持有上游），filter/map/take/skip 可任意叠加；
- map 允许改变元素类型（int → string 等）；
- 补终止操作：count / reduce / any / all；
- 同步更新 stream_test.cpp（覆盖式契约测试改为组合语义测试）、collection 设计文档、CHANGELOG。

### 2. process/ipc 测试补全 `[本分支: feat/stream-pipeline-ipc-tests]`

现状（盘点修正）：ipc 并非零测试，`subprocess_test.cpp` 里已有 10 个用例覆盖五个
原语的 happy path 与 remove 语义，但（a）放在 subprocess_test.cpp 文件里名不副实，
（b）以下缺口未覆盖。

目标：

- ipc 用例迁到独立的 `unittest/ipc_test.cpp`（subprocess_test.cpp 只留 Command/Child/AnonymousPipe）；
- 补错误路径：重复 create → ALREADY_EXISTS；open/connect 不存在的名字 → NOT_FOUND；
- 补契约用例：NamedPipeConnection 对端关闭后 read 返回 0（干净 EOF）、双工 echo 往返、
  write_all 大于缓冲的单块数据；
- 补 MessageQueue 多消息边界与顺序；
- 补 NamedSemaphore 阻塞 acquire（线程 release 唤醒）、initial_count 语义；
- 补 SharedMemory 生命周期（size/data/is_open/close）。

### 3. macOS 平台：要么支持，要么删死分支 `[决策]`

现状：`core/platform.hpp:16` 对非 `_WIN32`/`__linux__` 直接 `#error`，macOS 编不过全库；
但 `env/env.cpp` 写了完整的 `__APPLE__` 分支（`_NSGetExecutablePath`、`sw_vers`），
`crypto/random.cpp` 也有 `arc4random_buf` 分支——在当前平台宏下不可达。
CI 只有 ubuntu + windows。

选项：

- A（推荐）：正式支持 macOS——`platform.hpp` 放行 `__APPLE__`，审计 net/io/process 的
  平台分支，CI 加 macos runner。env/crypto 的 Apple 分支显然是为此预写的。
- B：明确不支持——删除不可达的 Apple 分支，platform.hpp 报错信息保持现状。

需要用户拍板；工作量 A ≫ B。

### 4. str/charset 非 Windows 补实现 `[待办]`

`charset.cpp:175` 起，非 Windows 平台 8 个编码转换函数全部返回 UNIMPLEMENTED。
计划：POSIX 侧用 iconv 实现（glibc 自带），Linux CI 验证。需要独立分支，
Windows 本机无法验证 POSIX 路径。

### 5. CA_TRY 的 MSVC 可用性 `[搁置]`

依赖 GNU statement expression，MSVC 不可用。**用户判断：实际使用率低，不处理。**
保留现状（宏与注释已说明限制）。

## 中优先级

- `[待办]` csv / ini 测试加厚：目前各 6 个头文件只有 1 个混合粗测，按 reader/writer/
  document/error 拆分用例。
- `[待办]` crypto 测试补位：hmac / hex / secure_random 无专属测试文件。
- `[待办]` ui 测试补位（button/control/capture_guard，仅 Windows 可跑）。
- `[待办]` 补设计文档：zip（EOCD 扫描/前缀恢复/ZIP64 取舍）、resources 与 i18n
  （构建期 rule 设计）优先；env/random/uuid/test 可接受没有。
- `[待办]` 删除 `core/wrapper.hpp` 的 deprecated Singleton（关联 issue #123，库内零使用）。
- `[待办]` libca.em：icm20948 补 .lua/.md（当前四件套不齐，不参与 em_driver 构建）。
- `[待办]` libca.em README 滞后：承诺 12 个模块章节只写了 4 节；总览表漏
  em_crypto/em_dstream/em_format/em_motion/em_mpool；驱动表漏 3 个且 24/28 未实物验证。
- `[待办]` libca.em 测试缺口：em_bus、em_driver 无单测。突破口是把 `em_platform/vhil.h`
  （目前只有 GPIO/I2C typedef、无实现）补成可用仿真层，驱动做寄存器级仿真测试。
- `[待办]` 已知功能缺口（代码内已声明，按需排期）：ymodem 发送方向、soft_i2c 从机模式。

## 低优先级 / 路线图

- `[待办]` 协议子集边界在 README 明示：http 无协议升级（WebSocket）、yaml 无锚点/别名/
  多文档、xml 无 DTD/PI，避免下游预期落差。
- `[待办]` 功能方向评估：fs 的 glob/通配匹配与目录监听；thread 的定时调度
  （scheduled executor）；time 模块加厚（自述「薄」）。
- `[待办]` 0.0.1 打 tag 发布，为后续不兼容变更建立参照系。

## 变更记录

- 2026-08-31：初版，来自全库盘点；高优先级 1/2 在 `feat/stream-pipeline-ipc-tests` 实施。
