# Changelog

本文件记录跨版本的不兼容变更与重要行为变化。当前版本 `0.0.1`。

## 格式约定

- 每个条目注明影响范围（libca / libca.em / 构建 / 全局）与升级注意事项
- 不兼容变更必须在合并前补充条目（见 README「不做严格兼容承诺」）

## [0.0.1] - 未发布

- 初始骨架：libca（C++17）+ libca.em（C99）双部分仓库，xmake 构建。

### libca

- **[opt] v2 重写**：`Arg` 移除 `short_name` / `has_value` 旧字段，取值类型唯一由
  `kind`（Flag/String/Int/StringList/Positional）表达，短名与多别名统一为
  `aliases` 列表；新增位置参数收集（`positionals()`）、Int 类型化校验、StringList
  追加语义、带默认取值重载。行为变更：未声明的裸 token 由静默丢弃改为报错，
  `--` 之后的 token 收集进 `positionals()`。该子库此前无下游用户，不提供迁移层。
  设计见 `doc/proposals/opt-v2.md`。
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
