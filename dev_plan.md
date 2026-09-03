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

### 3. macOS 平台：砍掉死分支 `[已完成：feat/stream-pipeline-ipc-tests]`

决策（用户拍板）：不做 macOS，采用方案 B。已删除 `env/env.cpp` 的
`_NSGetExecutablePath`/`sw_vers` 分支与 `crypto/random.cpp` 的 `arc4random_buf`
（BSD/Apple）分支——它们在 `core/platform.hpp` 的平台宏下不可达。
`platform.hpp` 注释已明确「有意只支持 Windows/Linux」。CI 维持 ubuntu+windows。

### 4. str/charset 非 Windows 补实现 `[已完成：feat/stream-pipeline-ipc-tests]`

`charset.cpp` 的 POSIX 存根（8 个函数返回 UNIMPLEMENTED）已替换为 iconv 实现：
GBK 用 glibc "GBK" 转换器，本地代码页取 `nl_langinfo(CODESET)`，wchar 用
"WCHAR_T"；非法/残缺序列返回 INVALID_ARGUMENT，转换对不被支持返回
UNIMPLEMENTED（裁剪系统缺 GBK 时测试跳过）。POSIX 路径由 ubuntu CI 验证。

### 6. fs::Path 路径值类型与编码边界收敛 `[本分支: feat/fs-path]`

现状：std::filesystem 的窄字符接口在 Windows 按 ACP 解析（中文路径有损），原对策是
每个调用点手写 u8path/generic_u8string——契约只在注释、u8path 在 C++20 废弃、
非法 UTF-8 行为实现定义且两平台不一致。

目标（已实现，待评审合并）：

- 新增 `ca::fs::Path`：内部持 `std::filesystem::path`，编码转换全经 `ca::str::OsString`
  （from_utf8 校验报 InvalidUtf8 / from_utf8_lossy / from_os_string / from_native），
  与 std::string 无隐式转换；fs 新增依赖 libca_str（fs 设计文档 §1 挂账的迁移评估落地）；
- FileUtil 全函数 Path 重载（返回路径的族返回 Path/vector<Path>），string 重载委托
  lossy——非法 UTF-8 收敛为定义清楚的 U+FFFD 替换；PathUtil 保留、内部经 Path；
- u8path/generic_u8string 全库 src 清零：csv/ini/json/toml/xml/yaml 文件流改经
  `Path::native()`（盘点修正：json/toml/xml/yaml 四模块的 u8path 此前漏记）；
- 设计记录见 fs设计文档 §4.8；fs 140 例测试全绿。

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

- 2026-09-03：新增第 6 项 fs::Path（分支 feat/fs-path）：Path 类型 + FileUtil 双重载族
  + u8path 全库清零；盘点修正——json/toml/xml/yaml 同样存在 u8path 调用（此前漏记）。
- 2026-08-31（第二批）：macOS 决策为砍掉（方案 B）并删除死分支；charset POSIX
  侧 iconv 实现完成——高优先级 1/2/3/4 全部落地。
- 2026-08-31：初版，来自全库盘点；高优先级 1/2 在 `feat/stream-pipeline-ipc-tests` 实施。
