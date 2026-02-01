代码审计指南

- 是否使用 `datatype.h` 中的固定宽度类型？
- 公共 API 是否包含 doxygen 注释（`@brief/@param/@return`）？
- 头文件是否使用 include guard（未使用 `#pragma once`）？
- `self` 指针是否使用 `param_check`？
- 错误码是否为负且命名规范？
- 是否避免动态分配并有合理的内存策略？
- 针对驱动，是否附带硬件验证计划或对可测试模块提供单元测试？
- 是否遵守风格规范（缩进、行长、命名等）？
