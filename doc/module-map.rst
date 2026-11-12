.. _module-map:

libca 模块地图
==============

模块面导航：每个头文件（及其 ``///`` 头注）属于哪个域、职责边界是什么、宏观规格在哪一篇。微观契约（参数、返回、失败模式）在对应头注里，本表只做落点。域内设计裁决见表格规格列指向的篇章；无独立规格页的模块，边界即头注。依赖分层（L0/L1/L2/L3 单向依赖）见 ``AGENTS.md``\ 。

地基（L0）
----------

.. list-table::
   :header-rows: 1
   :widths: auto

   * - 模块
     - 职责
     - 规格
   * - core
     - 全库地基：``Result`` / ``Status`` 错误值、``Bytes`` 字节缓冲（varint/zigzag）、``ScopeGuard``\ 、``DynamicLibrary``\ 、平台与栈工具
     - :ref:`core-design`\（字节缓冲 :ref:`core-design-bytes`\、错误值 :ref:`core-design-errors`\）

文本与容器（L1）
----------------

.. list-table::
   :header-rows: 1
   :widths: auto

   * - 模块
     - 职责
     - 规格
   * - str
     - UTF-8 类型族（拥有/视图/池化/惰性拼接）、``std::string`` 字节工具、编码内置化与代码页转换、format 门面
     - :ref:`str-design`\（类型族 :ref:`str-design-utf8-types`\、池化选型 :ref:`str-design-arena-pool`\、编码内置化 :ref:`str-design-charset-builtin`\）
   * - collection
     - 容器件：复杂度与所有权语义在头注，逐组件对齐 std 容器习惯
     - :ref:`collection-design`

系统与 IO（L2）
---------------

.. list-table::
   :header-rows: 1
   :widths: auto

   * - 模块
     - 职责
     - 规格
   * - fs
     - 路径值类型、文件读写（读路径走 C stdio）、原子写与错误值
     - :ref:`fs-design`\（错误模型 :ref:`fs-design-error-model`\、原子写 :ref:`fs-design-atomic-write`\、读路径 :ref:`fs-design-stdio-read`\）
   * - io
     - 统一同步字节流协议：Reader / Writer / Seek、缓冲层、原生 handle/fd RAII
     - :ref:`io-design`\（错误模型 :ref:`io-design-error-model`\、句柄所有权 :ref:`io-design-handle-ownership`\）
   * - time
     - 时间点/时长/时钟语义、``Stopwatch``\ 、定时与耗时测量
     - :ref:`time-design`\（时钟语义 :ref:`time-design-clock`\、溢出边界 :ref:`time-design-overflow`\）
   * - thread
     - 线程池、``StopSource``/``StopToken`` 协作取消、``BoundedQueue``\ 、``MessageLoop``\ 、Timer、EventBus
     - :ref:`thread-design`\（关闭状态机 :ref:`thread-design-shutdown`\、MessageLoop :ref:`thread-design-message-loop`\）
   * - process
     - 子进程生命周期与管道、IPC（内核消息队列 + 无内核对象共享内存环）
     - :ref:`process-design`\（子进程 :ref:`process-design-child-lifecycle`\、IPC :ref:`process-design-ipc`\、容错环 :ref:`process-design-shm-ring-queue`\）
   * - env
     - 环境变量与操作系统信息查询；Windows 上内部做 UTF-8 ↔ UTF-16 往返
     - 边界即头注（env.hpp），无独立规格页

数据格式与配置
--------------

格式族（json / toml / ini / yaml / csv / xml）同构：Document 内嵌 ``Utf8StringArena``\ ，DOM 字符串统一 ``Utf8StringRef``\ ，解析错误带行列位置；读写策略归各自 Options。

.. list-table::
   :header-rows: 1
   :widths: auto

   * - 模块
     - 职责
     - 规格
   * - json
     - JSON SAX/DOM 双形态 + ``KvStore`` 持久化键值
     - :ref:`json-design`\（Arena 架构 :ref:`json-design-arena`\、KvStore :ref:`json-design-kvstore`\）
   * - toml
     - TOML 子集解析与写回，Arena 架构的验证用例
     - :ref:`toml-design`\（所有权模型 :ref:`toml-design-ownership`\）
   * - ini
     - 保格式 round-trip 的 INI 读写
     - :ref:`ini-design`\（注释保留 :ref:`ini-design-format-keep`\、重复键策略 :ref:`ini-design-duplicate-policy`\）
   * - yaml
     - 配置子集，明确拒绝清单；解析器行预扫 + 行列游标
     - :ref:`yaml-design`\（拒绝清单 :ref:`yaml-design-reject-list`\）
   * - csv
     - 可配置分隔符的 DSV reader/writer，字节级字段（``intern_raw``\ ）
     - :ref:`csv-design`\（Arena 架构 :ref:`csv-design-arena`\、DSV 预设 :ref:`csv-design-dsv`\）
   * - xml
     - 配置子集的 XML DOM 与解析
     - :ref:`xml-design`
   * - config
     - 静态配置注册中心：强类型 ``ConfigVar`` 注册/查找，从 JSON 加载与监听
     - :ref:`config-design`\（加载语义 :ref:`config-design-load-semantics`\）
   * - zip
     - zip/gzip 读写与校验和
     - 边界即头注（zip_error.hpp / file.hpp），无独立规格页；错误模型迁移 ``Result`` 列入路线图

网络与协议
----------

.. list-table::
   :header-rows: 1
   :widths: auto

   * - 模块
     - 职责
     - 规格
   * - net
     - 同步网络基础设施：socket/TCP/UDP、DNS 与缓存、``TlsStream``\ （with_openssl 门控）
     - :ref:`net-design`\（错误模型 :ref:`net-design-error-model`\、DNS 缓存 :ref:`net-design-dns-cache`\、TLS :ref:`net-design-tls`\）
   * - http
     - HTTP 报文层与 client/server，建立在 net/io 之上
     - :ref:`http-design`\（framing 安全 :ref:`http-design-framing`\）

安全与随机
----------

.. list-table::
   :header-rows: 1
   :widths: auto

   * - 模块
     - 职责
     - 规格
   * - crypto
     - 哈希/HMAC/KDF/编码、AES 多后端（内置参考实现 + OpenSSL/CNG）
     - :ref:`crypto-design`\（后端选择 :ref:`crypto-design-backend`\、安全边界 :ref:`crypto-design-security-boundary`\）
   * - random
     - 随机源两层：系统安全随机与可复现引擎；上游逐位收敛契约
     - :ref:`random-design`\（收敛契约 :ref:`random-design-canonical`\）
   * - uuid
     - UUID v4 生成与校验，随机源复用 ``crypto`` CSPRNG
     - 边界即头注（uuid.hpp），无独立规格页

观测与诊断
----------

.. list-table::
   :header-rows: 1
   :widths: auto

   * - 模块
     - 职责
     - 规格
   * - log
     - 门面/后端分离的日志：``LoggerRegistry`` 按 target 分发、级别收归 Logger、spdlog 可选后端
     - :ref:`log-design`\（fmt 解耦 :ref:`log-design-opaque-format`\、分发竞态 :ref:`log-design-registry`\）
   * - opt
     - 命令行解析：Command 树、互斥组、初值注入优先级、HelpTable 排版层
     - :ref:`opt-design`\（优先级 :ref:`opt-design-priority`\、互斥组 :ref:`opt-design-mutex-groups`\）

嵌入与国际化
------------

构建期生成、运行时零文件依赖的两个 header-only 件：边界即各自头注，无独立规格页。

.. list-table::
   :header-rows: 1
   :widths: auto

   * - 模块
     - 职责
     - 规格
   * - i18n
     - CLI 消息国际化运行时：翻译表由 xmake rule 从 ``translations/*.lang`` 生成 constexpr 头嵌入
     - 边界即头注（i18n.hpp）
   * - resources
     - 嵌入资源运行时：目录树由 xmake rule 生成 constexpr 头嵌入，按 bundle 查询
     - 边界即头注（resources.hpp）

测试基建
--------

libca.test（test.hpp，header-only）：按标记文件的多项目测试布局与 test_resource 样本定位，不属库面，随测试构建。
