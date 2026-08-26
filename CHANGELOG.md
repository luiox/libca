# Changelog

本文件记录跨版本的不兼容变更与重要行为变化。当前版本 `0.0.1`。

## 格式约定

- 每个条目注明影响范围（libca / libca.em / 构建 / 全局）与升级注意事项
- 不兼容变更必须在合并前补充条目（见 README「不做严格兼容承诺」）

## [0.0.1] - 未发布

- 初始骨架：libca（C++17）+ libca.em（C99）双部分仓库，xmake 构建。

### libca

- 新增模块：`env`（环境变量/系统信息）、`random`（CSPRNG 随机数）、`uuid`（UUID v4）、
  `opt`（命令行选项解析）。
- **[zip] 新模块**：JVM `ZipFile` 语义只读访问器（最后合法 EOCD/前缀拼接恢复/ZIP64）
  与流式读写（`ZipInputStream`/`ZipOutputStream`，DD 条目支持）；zlib 经 xrepo，
  根开关 `with_zip=n` 可整体跳过（无 zlib 环境不影响其余部分）。
- **[resources] 新模块**：清单驱动 constexpr 目录树嵌入 + header-only 查询运行时
  （bundle 隔离/二分查找/前缀过滤迭代），构建 rule 全局注册名
  `libca.resources.embed`。
- **[i18n] 新模块**：`.lang` 构建期嵌入（rule `libca.i18n.embed-lang`，zh_CN 超集校验）
  + tr/trf 回退运行时（当前语言 → zh_CN → key 原样）。
- **[test] 新模块**：`.project_root_file` 标记扫描与多项目 test_resource 定位、
  `<top>/test` 输出根约定（环境变量 `LIBCA_TEST_OUT_ROOT` 可覆盖）。
- **[core] 新增 `tag_cast.hpp`**：整数 tag 层级的 LLVM 风格 `isa`/`cast`/`dyn_cast`
  （`TypeOf<T>` 特化接入），作为与 `Polymorphic` 并存的第二种 RTTI-free 机制，
  位于 `ca::core::tag_cast` 命名空间。
- `str`：新增 `OsString`/`OsStr` 平台原生字符串；新增 `format`/`format_to`/`format_runtime`
  格式化门面（fmt 转为 str 的 public 依赖，下游经 `add_deps("libca_str")` 间接获得）。
- `thread`：新增 MPSC 通道 `channel`；新增 `OnceCell`/`OnceLock` 延迟初始化。
- `csv`：新增 DSV 预设 `csv()` / `tsv()` / `delimited(char)`。
- **log 模块重新设计**：门面/后端分离，fmt 依赖收敛到门面侧；`LoggerRegistry` 按 target
  分发取代单一全局 logger；级别管理收归 `Logger`；新增零依赖 `SimpleLogBackend` 与可选
  spdlog 后端。升级注意：旧 `set_global_logger` 全局接口移除，迁移到
  `LoggerRegistry::register_logger` + `CA_LOG_*`/`CA_LOGT_*` 宏入口。
- **不兼容改名**：`Level::Error` → `Level::Error_`，规避 Windows `wingdi.h` 的 `ERROR`
  宏冲突；受影响代码需同步改名。
- **[opt] v2 重写**：`Arg` 移除 `short_name` / `has_value` 旧字段，取值类型唯一由
  `kind`（Flag/String/Int/StringList/Positional）表达，短名与多别名统一为
  `aliases` 列表；新增位置参数收集（`positionals()`）、Int 类型化校验、StringList
  追加语义、带默认取值重载。行为变更：未声明的裸 token 由静默丢弃改为报错，
  `--` 之后的 token 收集进 `positionals()`。该子库此前无下游用户，不提供迁移层。
  设计见 `libca/opt/doc/opt设计文档.md`。
- **[opt] v2-P1**：`parse()` 返回类型改为 `Result<ParseResult, ParseError>`
  （类别 + 出错选项名 + 现成描述，`to_status_code()` 提供到 `StatusCode` 的桥接；
  原 `--help` 的 CANCELLED+文本模式迁移为 `HelpRequested` 类别）；新增互斥组
  （`Command::mutex_groups`）、选项分组渲染（`Arg::group`）、自定义 usage 行
  （`Command::usage`）。
- **[opt] v2-P2**：新增带初始值注入的 `parse()` 重载（优先级
  default < 注入初值 < 命令行，仅带值选项参与，required/互斥组视为已提供）；
  新增 `help_text(cmd, groups)` 分组过滤帮助渲染与 `Parser::root()`
  元数据只读访问（schema 导出由下游自建）。
- **[opt] v2 评审修正**：新增 `ParseResult::source_of()` 值来源查询
  （`ValueSource::{None,CommandLine,Initial,Default}`）；修复互斥组与默认值/
  注入初值叠加时的误报——冲突/缺失判定只把命令行或注入初值算作「选择」，
  静态默认不再触发 MutexConflict 或短路 required 组。
- **[opt] OptionalString**：新增可选值形态 `OptKind::OptionalString`——
  值仅经内联（`--dump=x`）或短选项附着（`-dx`）提供，裸出现视为已提供且值为
  空串；空格形态不消费后继 token，杜绝与位置参数/子命令的歧义。适用于
  "不带值输出 stdout、带值写文件"类选项。
- **[opt] 互斥组标识**：`MutexGroup` 新增 `label`，互斥类错误经
  `ParseError.group` 回填组标识（无 label 时为成员名拼接），下游按组分派文案；
  `group` 字段置于结构体末尾，三元素聚合初始化写法不受影响。
- **[opt] 修复子命令 usage 行**：子命令层 `--help` 曾输出 `Usage: <sub> <sub>
  [options]`（子命令名重复且程序名缺失），改为 `Usage: <程序名> <子命令路径>
  [options]`；自定义 `Command::usage` 由「前置子命令路径 + usage」改为完整替换
  （与头文件声明一致，程序名由定义方书写）。根命令与 `help_text()` 输出不变。
- **[opt] 内置 -h/--help 可被覆盖**：`--help` / `-h` 注册为某选项的长名/别名时
  （如 `-h` 作 `--host` 的别名），按该选项定义解析，不再触发内置帮助；未注册时
  行为不变。此前注册这两个 token 会静默失效。
- **[opt] Int 严格化**：选项值与注入初值含空白即报 InvalidInteger（`" 30"`、
  `"30 "` 对称拒绝）。此前 `strtoll` 跳过前导空白，`" 30"` 被接受而 `"30 "` 报错；
  显式正负号（`+30`）不受影响。
- **[opt] 跨层级同名选项优先级**：父命令与子命令注册同名选项（值键跨层级共享）
  时，父命令行显式给值不再被子命令的 default/注入初值静默覆盖——
  「default < 注入初值 < 命令行」的全局优先级在跨层级场景同样成立；
  子命令的同名显式出现仍按 last-wins 生效。
- **[core] Result 存储对齐修复**：`Storage::Align` 由「跟随尺寸更大者的 alignof」
  改为 `max(alignof(T), alignof(E))`——`Result<array<char,40>, string>` 等尺寸
  相近而 alignof 悬殊的组合曾取到 1，对齐不足属严格 UB（x86 潜伏，MSVC debug /
  ARM 可能崩溃）。补双向 static_assert。
- **[str] UTF-8 转换校验续字节**：`utf8_to_utf16/_utf32/_latin1` 全系列与
  `to_wstring` 此前不校验续字节，「合法首字节 + 非法续字节」被误解码成错误码点
  （违反「返回 0 / 抛异常」契约）。新增 `utf8_valid_continuation` 助手并接入
  全部逐序列解码循环，与 `utf8_is_valid` 的校验口径一致。
- **[log] registry 退休列表保活**：被替换/注销/clear 的旧 Logger 此前随 map
  释放最后引用即刻析构，而调用链在锁释放后仍持裸指针——并发注销（典型：
  shutdown 阶段 `clear()` 与打日志并发）use-after-free。旧 Logger 现移入退休
  列表（进程生命周期保活），兑现头文件承诺的 `get()` 裸指针安全契约。
- **[ini] 解析健壮性四项**：非法 UTF-8 报 ParseError（此前 `read()` 抛异常击穿
  Result 契约，GBK 存量配置直接 terminate 调用方）；行内注释先于分隔符出现时
  注释文字里的 `=`/`:` 不再被误当分隔符吞进 key；跳过 UTF-8 BOM（此前首
  key/section 带不可见 U+FEFF 前缀）；`get_int`/`get_double` 拒绝前导空白
  （此前 `" 42"` 合法而 `"42 "` 报错）。
- **[json] 解析入口整体校验 UTF-8**：字符串值里的非法 UTF-8 此前经 arena
  intern 静默变空串、解析"成功"（RFC 8259 要求 JSON 文本为合法 UTF-8），
  数据无声丢失；现统一报错。
- **[xml] 解析入口整体校验 UTF-8**：元素名/属性值/文本里的非法序列此前静默
  变空（曾产出空名元素的损坏 DOM），而注释/CDATA 原样保留；现统一报错。
- **[csv] read 跳过 UTF-8 BOM**：首个 header 字段此前带不可见 U+FEFF 前缀，
  按名取列全部失配且无报错。
- **[thread] 公共头 `std::max` 加括号**：`bounded_queue.hpp` 中未加括号的
  `std::max` 在下游先包含 `<windows.h>`（未定义 NOMINMAX）时被函数式 `max`
  宏改写、包含即编译失败。
- **[fs] move 不再预删目标**：覆盖移动此前先 `remove_all(dst)` 再 rename，
  目标为目录时整棵子树被删而源 rename 失败，数据白白丢失；也不存在"原子
  替换"窗口。现 rename 失败即返回 false，文件目标的覆盖由系统原子替换
  完成（Windows 走 MOVEFILE_REPLACE_EXISTING）。overwrite=false 语义不变。
- **[fs] glob 裸相对模式修复**：`*.txt` 等不含斜杠的相对模式此前用完整路径
  拼接候选再匹配，永远失配；现候选 = 模式前缀 + 文件名，与模式形状对齐。
- **[str] 增补平面码点防 wint_t 截断**：Windows 上 `wint_t` 为 16 位，
  `Utf8Char::is_*`/`to_*` 直接 `static_cast<wint_t>(cp_)` 会把 U+10000 以上
  码点截断成错误 BMP 码点（如 U+10061 判成 'a' 可转大写）。超 BMP 码点现
  短路返回"非字母数字"类结果。`to_lower_case`/`to_upper_case` 同步修复
  `::tolower(char)` 负值 UB（现经 `unsigned char` 转换）。
- **[crypto] 密钥材料离场清零**：hmac/chacha20/rc4 计算后密钥块、轮状态与
  S-box 残留在栈/堆上不清零，可被交换/转储/复用内存读到。新增
  `crypto_util::secure_zero`（volatile 逐字节写，防优化器消除），计算完
  即擦除全部密钥派生材料。
- **[toml] 对齐 TOML 1.0 四项**：`[a.b]` 后定义 `[a]` 超表合法（此前误拒）；
  进制整数先校验字符集再 strtoll（`0x-5`/`0x 5` 此前被解析成奇怪值而非
  报错）；`1e_2` 类指数 mark 后下划线拒绝；writer 转义 DEL（0x7F，
  TOML 1.0 不允许原样出现在基本字符串中）。
- **[yaml] `\xXX` 转义按码点编码**：XX >= 0x80 时此前按原始单字节追加产生
  非法 UTF-8，经 arena intern 静默变空串（字符串无声丢失）；YAML 1.2 中
  `\xXX` 是码点转义 U+00XX，现编码为对应 UTF-8 序列。
- **[http] 协议错误响应防 RST**：400/408/413/431/501 错误路径此前直接关闭
  连接，Windows 上未读入站数据（如客户端仍在推送 body 时的 413）触发 RST
  吞掉已发送的响应；现与过载路径一致——shutdown 写半侧后排空读侧至 EOF
  或期限。`Expect: 100-continue` 仅 HTTP/1.1 回 interim（1.0 无此语义，
  此前会回出 "HTTP/1.0 100 Continue"）。client 复用的 keep-alive 连接被
  服务器关闭时（idle 回收常态），幂等方法在响应任何字节前遇 EOF/reset
  自动在新连接上重试一次（RFC 9110 9.2.2）；非幂等方法维持报错。
- **[libca] 格式化模块文件路径经 u8path**：json/xml/csv/ini/yaml/toml 的
  `read_file`/`write_file` 此前把 UTF-8 路径按窄字符传给 fstream，Windows
  ACP 非 UTF-8（中文环境默认 GBK）时非 ASCII 路径打开失败或落到错误文件；
  现统一经 `std::filesystem::u8path`，与 `libca/fs` 做法一致。
- **[toml] 解析入口整体校验 UTF-8**：与 json/ini/xml 对齐——字符串值里的非法
  UTF-8 此前经 arena intern 静默变空串、解析"成功"（TOML 1.0 要求文件为合法
  UTF-8），数据无声丢失；现入口统一报错。
- **[yaml] 解析入口整体校验 UTF-8**：同上，plain/引号标量里的非法序列此前
  静默变空串；现入口统一报错。
- **[json] float 整数形态补 `.0`**：writer 用 `%.17g` 输出整数形态的 float
  （`2.0` 写成 `"2"`）读回被当 Int、`-0.0` 的符号与类型双丢；现与 toml/yaml
  writer 一致，整数形态追加 `.0` 保持 Float。
- **[str] moved-from Utf8String 转 string_view 回落空视图**：移动后
  `data_` 为 nullptr，`operator std::string_view` 此前直接构造
  `string_view(nullptr, 0)`（UB）；现与 Utf8StringRef/ZUtf8StringRef 同口径
  回落默认构造的空视图。
- **[collection] Stream filter/map 覆盖语义显式化**：头文件以 `@warning`
  声明 filter()/map() 是覆盖式而非链式组合（再次调用替换先前谓词），并补
  钉住该契约的测试；行为本身不变。
- **[core] 新增 `Option<T>`**：Rust 语义的可空值类型（内部包装 `std::optional`），
  `Some`/`None` 工厂与哨兵，`unwrap`/`expect`/`map`/`and_then`/`or_else`/`take`/
  `unwrap_or` 系列与 `ok_or<E>()` 转 `Result`；`Result::ok()/err()` 反向桥接
  （`T` 为 void 时不参与）。与 `Result` 同居 `ca::core`，`ca` 顶层重导出。
- **[crypto] sha3 未对齐输入 UB 修复**：`process_block` 此前把数据指针直接
  cast 成 `const uint64_t*` 按道读取，缓冲非 8 字节对齐（堆上偏移切片）时属
  严格别名/对齐 UB（x86 潜伏，ARM 可能崩溃）；改按 8 字节 word `memcpy`
  混入，编译器会优化回单条加载。补非对齐缓冲摘要不变的钉住测试。
- **[toml] 浮点与表重开严格化（TOML 1.0 对齐）**：`1.` / `.5` / `1.e2` 等
  `.` 一侧缺数字的浮点形态此前被宽容接受（`strtod` 家族行为），TOML 1.0 要求
  两侧均有数字，现报错；dotted-key 创建的表不可被 `[header]` 重开或作为
  header 路径段打开，header 显式定义的表不可被 dotted-key 扩展，inline
  table 全封闭（header/dotted-key 均不可扩展）——此前部分场景静默允许，
  现按 spec 拒绝，杜绝数据模型歧义。
- **[process] IPC unlink API 与长度封顶**：新增
  `remove_shared_memory`/`remove_semaphore`/`remove_message_queue`
  （POSIX `shm_unlink`/`sem_unlink`/`mq_unlink`，ENOENT 幂等成功；Windows
  侧为带命名校验的 no-op，内核对象随句柄关闭回收）；管道 `read`/`write`
  系列长度参数按 `SSIZE_MAX` 封顶（POSIX）或 DWORD 范围校验（Windows），
  超限报错而非静默截断；`NamedSemaphore::release` 拒绝超出 LONG_MAX 的
  count，限时等待时长 DWORD 溢出钳制。
- **[xml] 默认 `max_depth` 1000→256**（行为变更）：实测 Windows 默认 1MB
  线程栈下 ~850 层嵌套即栈溢出，原默认 1000 的守卫来不及触发（守卫本身
  递归）；256 与 libxml2 `XML_MAX_DEPTH` 一致，正常文档不受影响。需要更深
  嵌套的特殊场景须显式调大选项并同时增大线程栈。
- **[str] Utf8String 错误通道**：新增 `try_from_data`/`try_from_cstr`，返回
  `StatusResult<Utf8String>`，非法 UTF-8 报 `INVALID_ARGUMENT` 而非静默
  产出空串；`from_cstr` 头文件补 `@warning` 显式声明吞错语义。
- **[str] wstring/cstring Doxygen 补全**：两个头文件的公共 API 注释补齐
  （此前部分接口无文档）；行为不变。
- **[str] 操作类 API 测试空白补齐**：`starts_with`/`ends_with`/`trim`/
  `split`/`replace_all`/`to_lower`/`to_upper` 此前在 Utf8StringRef（含
  拥有型 Utf8String）与 `CString`/`WString` 系均零测试覆盖，现双侧补齐
  （视图侧同时钉住"指向原数据"的非拥有语义与 split 的空片段/空分隔符
  边界）。
