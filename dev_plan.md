- `[待办]` csv / ini 测试加厚：目前各 6 个头文件只有 1 个混合粗测，按 reader/writer/ document/error 拆分用例。
- `[待办]` crypto 测试补位：hmac / hex / secure_random 无专属测试文件。
- `[待办]` ui 测试补位（button/control/capture_guard，仅 Windows 可跑）。
- `[待办]` 补设计文档：zip（EOCD 扫描/前缀恢复/ZIP64 取舍）、resources 与 i18n （构建期 rule 设计）优先；env/random/uuid/test 可接受没有。
- `[待办]` 删除 `core/wrapper.hpp` 的 deprecated Singleton（关联 issue #123，库内零使用）。

- `[待办]` 协议子集边界在 README 明示：http 无协议升级（WebSocket）、yaml 无锚点/别名/ 多文档、xml 无 DTD/PI，避免下游预期落差。
- `[待办]` 功能方向评估：fs 的 glob/通配匹配与目录监听；thread 的定时调度（scheduled executor）；time 模块加厚（自述薄）。

