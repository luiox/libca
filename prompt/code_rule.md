【代码库规范（必须遵守）】

【语言】
- C99
- 禁止使用C++语法，但是要以下面这个包裹
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
- 使用`datatype.h`中的固定长度类型，例如`i8`, `u8`, `i16`, `u16`, `i32`, `u32`, `i64`, `u64`, `f32`, `f64`, `usize`, `bool`。而不是`int`, `short`, `long`这些类型。仅仅在表达字符和字符串时候使用`char`。

【命名规范】
- 文件命名：{snake_case}
- 类型命名：{struct xxx / enum xxx_enum}，结构体需要提供一个xxx_t的别名，枚举需要提供xxx的别名，使用别名而不是struct xxx这种
- 函数命名：{模块前缀 + 动词}
- 宏命名：{全大写 + 前缀}，类似于函数的宏应该参考函数命名，例如array_size，其他均为全大写加下划线
- 变量命名：全局变量需要前缀`g_`
- 枚举命名：枚举类型以`_enum`结尾，而且要snake_case给出`xxx_enum`，然后typedef出xxx。枚举的值可以是大写然后下划线分割。
- 使用oop设计的代码：函数命名以`类名_行为`的方式命名，其他则以`动词_名词`的方式组织
- 对于源文件内私有函数，必须加static修饰
- 头文件保护：使用`#ifndef FILENAME_H`，禁止使用`#pragma once`。头文件保护的宏以`项目名_路径_文件名_H`的方式定义
- 头文件保护：C语言标准库头文件以`#include <stdio.h>`的方式包含，而内部则以`#include "file.h"`的方式包含

【代码风格】
- 缩进：{4 空格}
- 花括号风格：{K&R}
- 每行最大长度：{120}
- 剩下参考仓库根目录的.clangformat文件

【注释规范】
- 公共 API：{需要 doxygen}
- 私有函数：{必须有注释}
- 使用中文编写注释

【单元测试规范】
- 对于每个模块的单元测试，应该写在对应的源文件的最下面
- 所有测试代码应该是由`#if TEST_ENABLE ... #endif`包裹
- 单元测试的框架是基于`../em_test/test.h`，创建测试目标以后，使用`add_rules("em_test", { test_enable = true, use_default_main = true })`即可自动注入main函数
- 应该可以在windows、linux上进行mock和单元测试确保逻辑没有错误
- 样例
  ```c
  /* ... 模块代码实现 ... */

  #if TEST_ENABLE
  #include "../em_test/test.h"

  TEST_CASE(module_feature_name) {
      // Setup
      // Action
      // Assert (TEST_ASSERT_EQUAL_INT, etc.)
  }
  #endif
  ```
- 测试目标的名字应该是`test-模块名`，并且使用`xmake run 测试目标名字`来运行测试，保证测试通过所有测试用例

