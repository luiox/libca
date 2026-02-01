# PR 检查清单（Reviewer 使用）

- [ ] 代码遵循 `prompt/em_code_rule.md`（类型、命名、注释、头文件保护等）。
- [ ] 公共 API 包含 doxygen 注释（`@brief/@param/@return`）。
- [ ] `xxx_t *self` 等关键接口使用 `param_check` 进行检查（或在文档中说明为何不需要）。
- [ ] 错误码以 `MODULE_ERR_*` 命名并为负数；存在 `MODULE_OK`（0）时需定义。
- [ ] 头文件未使用 `#pragma once`，使用项目约定的 include guard。
- [ ] 驱动不包含单元测试（若包含测试内容，请确认为可独立模块的单元测试并在 PR 中说明理由）。
- [ ] 提供硬件验证计划或测试结果（针对本驱动），或链接到测试日志/截图。
- [ ] 无未必要的全局变量；如需全局变量，名称以 `g_` 前缀。
- [ ] 行长 ≤ 120，缩进 4 空格，K&R 花括号风格。
- [ ] 对于重要改动，更新相应文档（README 或 references 文档）。
