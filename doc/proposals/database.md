# 提案：Database / 连接池

## 背景

旧 `libca.core/src/database/` 是占位实现，依赖 `<mysql/mysql.h>`，且有多处严重 bug：

- `Connection::Connection()` 调 `mysql_init(nullptr)` 不检查 null，后续所有方法空指针解引用
- `Connection::query` 用 `mysql_use_result`（流式），调用方必须自行 `mysql_free_result`，
  所有权没有约定
- `Connection::refeshAliveTime` 拼写错误（应为 `refreshAliveTime`）
- `ConnectionPool::loadFromConfigFile` 是 stub，**永远返回 true**，根本不读 INI 文件，
  因此 `m_ip` / `m_port` 等配置字段全是默认值，连接池无法实际工作
- 生产者任务 `productConnectionTask` 死循环；扫描任务 `scannerConnectionTask` 在
  `m_connQueue.front()` 前不检查 `empty()`，存在 UB
- 用 detached thread 跑生产/扫描任务，无 shutdown，静态析构顺序 UB
- `getConnection` 返回 `shared_ptr<Connection>`（自定义 deleter 捕获 `this`），
  单例销毁后 deleter 解引用悬挂 `this`
- `getAliveTime` 用 `clock()`（CPU 时间）和 wall-clock 超时比较，语义错误
- 公开 API 返回 `shared_ptr<Connection>` 但队列里存裸 `Connection*`，所有权模型混乱
- 头文件保护符拼写错误（`LIBCA_DATABSE_...` 缺一个 A）

旧代码不可用，已随 `libca.core/` 删除。本提案记录未来在新 `libca/` 中实现数据库
连接池的设计意图。

## 建议落点

新建 `libca/db`（或 `libca/sql`）模块，命名空间 `ca::db`。

## 设计要点

1. **依赖**：需要 mysql client 库。xmake 通过 `add_requires("libmysqlclient")`
   引入（仓库目前未 vendor 该依赖）。建议做成可选模块（类似 `with_openssl` 模式），
   通过 `with_db` option 控制，避免默认强制拉取。
2. **基于新库原语**：
   - 错误用 `ca::core::Status` / `StatusResult<Row>` 反馈
   - 连接池后台任务用 `ca::thread::Thread` + `StopToken`，提供显式 `shutdown()`，
     避免 detached thread 与静态析构顺序问题
   - 配置加载可复用 `libca/ini`（已有 INI 解析器）
3. **核心抽象**：
   - `Connection`：RAII 包装 `MYSQL*`，移动语义，析构自动 close
   - `ConnectionPool`：显式构造（传入 config 结构体，避免单例 + 隐式加载文件），
     `acquire() -> StatusResult<Connection>`，提供 `shutdown()`
   - `Result` / `Row` / `PreparedStatement` 等（按需）
4. **修复旧代码问题**：
   - 单例 + 隐式配置文件加载改成显式构造注入配置
   - 统一所有权（池内部用 `unique_ptr<Connection>`，外部借出用 RAII 句柄）
   - 用 `steady_clock` 而非 `clock()` 衡量空闲时间
   - 生产/扫描任务必须可停止，与池生命周期绑定

## 不在范围

- 本提案不规定精确 API 签名，留给具体设计阶段决定。
- 是否支持 PostgreSQL / SQLite 等多后端待讨论。
