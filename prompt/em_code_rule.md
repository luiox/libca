【libca的em_系列组件代码库规范（必须遵守）】
下面的代码规范仅限于`src/em_xxx`这些目录下的所有c代码（包含其子目录）

【语言】
- 仅使用标准的C99（在`.c`和`.h`这两种c文件内），禁止编写c++代码；使用编译器扩展特性必须显式标记，除了`em_base/compiler_compat.h`内对编译器扩展特性宏的封装
- 在c文件内，禁止使用c++语法，并且不要用c++编译器编译C代码，只能是用c代码的接口；对外公开的头文件必须用下面的方式包裹C接口声明

```c
#ifdef __cplusplus
extern "C" {
#endif

#ifdef __cplusplus
}
#endif
```

【代码质量目标】
- 可靠和实用
- 可测试
- 可维护

【类型系统】
- 使用`datatype.h`中的固定长度类型，例如`i8`, `u8`, `i16`, `u16`, `i32`, `u32`, `i64`, `u64`, `f32`, `f64`, `usize`, `bool`。而不是`int`, `short`, `long`这些类型。仅仅在表达字符和字符串时候使用`char`。`datatype.h`中的`bool`类型是C99的`<stdbool.h>`
- 不直接使用`size_t`、`uintptr_t`等标准整数类型，统一使用`usize`或对应的固定长度类型
- 索引/长度/容量使用`usize`；偏移量或可能为负的差值使用`i32`
- 非必要尽可能不要用`i64`，有的平台可能没有`i64`

【命名规范】
- 文件命名：{snake_case}
- 类型命名：{struct xxx / enum xxx_enum}，结构体需要提供一个xxx_t的别名，枚举需要提供一个xxx_t的别名，使用别名而不是struct xxx这种
- 函数命名：{模块前缀 + 动词}，模块前缀以模块名为准，例如`log_xxx`
- 宏命名：类似于函数并且是给用户使用的宏也使用snake_case，例如`array_size`。否则使用全大写加下划线分割，并且前缀`CA_`
- 变量命名：全局变量需要前缀`g_`
- 枚举命名：枚举类型的名字要满足是以snake_case给出`xxx_enum`，然后typedef出xxx_t。枚举的值使用大写+下划线并带模块前缀以避免冲突。
- 使用oop设计的代码：函数命名以`类名_行为`的方式命名，其他则以`动词_名词`的方式组织
- 对于源文件内私有函数（仅在当前.c文件使用且不在头文件声明），必须加static修饰
- 头文件保护：禁止使用`#pragma once`。头文件保护的宏以`项目名_路径_文件名_H`的方式定义
- 头文件包含：
  - C语言标准库头文件必须使用尖括号形式，例如`#include <stdio.h>`
  - 对于`em_xxx`模块之间的依赖（跨模块），必须使用“外疏”写法：`#include <em_xxx/yyy.h>`
  - 对于同一模块内部头文件（模块内依赖），必须使用“内亲”写法：`#include "yyy.h"`或相对路径（如`#include "../yyy.h"`）
  - 禁止把跨模块头文件写成`#include "em_xxx/yyy.h"`

【代码风格】
- 缩进：仅使用4空格，禁止使用tab。
- 花括号风格：{K&R}
- 每行最大长度：{120}
- 剩下参考仓库根目录的.clangformat文件；若与本文冲突，以本文为准

【注释规范】
- 使用中文编写注释，但是不要把英文技术术语翻译为中文
- 头文件（.h）原则：所有对外 API 函数必须在头文件中使用 Doxygen 格式完整注释（至少包含 @brief, @param, @return，如果有额外说明则应该有@note）。
- 源文件（.c）原则：对于在 .h 中已声明的函数：禁止在 .c 的函数定义前重复复制 Doxygen 注释。
- 对于 static 内部函数：必须在定义处添加 Doxygen 注释，详细说明输入输出及逻辑。
- 逻辑注释：在函数体内部，对于非直观的逻辑（位操作、公式、状态机），必须使用行内注释解释。

【代码细节规范】
- 函数参数要做空指针检查，针对self指针（如`xxx_t *self`），尤其是底层的driver模块，不要用if判断做检查，而是契约式编程，用`em_base/debug.h`的`param_check`宏检查。其他非热路径以及可能由用户传NULL的用if判断做检查并返回错误码。

【单元测试规范】
- 对于每个模块的单元测试，应该写在对应的源文件的最下面，并且仅在`TEST_ENABLE`下编译，不影响发布构建
- 所有测试代码应该是由`#if TEST_ENABLE ... #endif`包裹
- 单元测试的框架是基于`<em_test/test.h>`，创建测试目标以后，使用`add_rules("em_test", { test_enable = true, use_default_main = true })`即可自动注入main函数
- 应该可以在windows、linux上进行mock和单元测试确保逻辑没有错误；mock需使用`em_test`提供的替身能力
- 样例
  ```c
  /* ... 模块代码实现 ... */

  #if TEST_ENABLE
  #include <em_test/test.h>

  TEST_CASE(module_feature_name) {
      // Setup
      // Action
      // Assert (TEST_ASSERT_EQUAL_INT, etc.)
  }
  #endif
  ```
- 测试目标的名字应该是`test-模块名`（模块名以源文件名去掉扩展名为准），并且使用`xmake run 测试目标名字`来运行测试，保证测试通过所有测试用例

