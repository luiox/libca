- `[待办]` csv / ini 测试加厚：目前各 6 个头文件只有 1 个混合粗测，按 reader/writer/ document/error 拆分用例。
- `[待办]` crypto 测试补位：hmac / hex / secure_random 无专属测试文件。
- `[已迁出]` Windows UI 测试（Button、Control、CaptureGuard）归独立 uikit 仓库维护。`libca/ui` 从 0.0.9 起移出本仓库，迁移记录见 [CHANGELOG](CHANGELOG.md#009---2026-10-01)。
- `[待办]` 补设计文档：zip（EOCD 扫描/前缀恢复/ZIP64 取舍）、resources 与 i18n （构建期 rule 设计）优先；env/random/uuid/test 可接受没有。
- `[待办]` 删除 `core/wrapper.hpp` 的 deprecated Singleton（关联 issue #123，库内零使用）。

- `[待办]` 协议子集边界在 README 明示：http 无协议升级（WebSocket）、yaml 无锚点/别名/ 多文档、xml 无 DTD/PI，避免下游预期落差。

## 功能方向评估

- **fs glob（已具备）**。现有 `FileUtil::glob` 支持 `*`、`?`、`**` 和 `**/`，已覆盖普通及递归文件匹配。匹配器的字节与路径分隔符语义由现有测试固定。出现只匹配路径字符串的调用场景时，再考虑把内部匹配器作为独立 API 暴露。
- **fs 目录监听（有需求时立项）**。首版可为 Windows 与 Linux 提供可取消的单目录监听，使用 `fs::Path` 返回相对路径，覆盖新增、修改和删除。重命名先按删除和新增呈现，事件溢出时显式要求调用方重新扫描。递归监听需要处理 Linux 新子目录注册窗口，留待首版契约验证后再评估。[Windows API](https://learn.microsoft.com/en-us/windows/win32/api/winbase/nf-winbase-readdirectorychangesw) · [Linux inotify](https://man7.org/linux/man-pages/man7/inotify.7.html)
- **thread 定时调度（暂缓新增 executor）**。`MessageLoop::post_delay_task` 支持串行延迟任务，`TimerManager` 支持可取消的单次任务和固定延迟重复任务。需要到期后并行执行时，先用 `TimerManager` 将工作转交 `ThreadPool::try_submit`，由调用方处理池满。若多处需要共享封装，先明确未到期与已入池任务的取消、满队列处理、关闭顺序和 future 的完成方式。
- **time 扩展（优先值类型边界）**。现有 `Duration`、`Timestamp`、`Stopwatch` 和 `ScopeTiming` 覆盖基础算术与单调计时。下一批优先评估带溢出检查的构造与算术，明确负时间戳的舍入语义，用 epoch 前后的值与整数边界做回归用例。等有跨时区业务需求后再评估时区数据库和日历能力。
