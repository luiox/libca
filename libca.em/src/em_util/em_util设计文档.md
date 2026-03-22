# em_util 设计文档

## 目标

`em_util` 提供常用算法与数据结构工具，定位是可复用、零动态内存依赖、便于嵌入式裁剪。

## 设计原则

1. 纯 C 实现，避免额外运行时依赖。
2. 文件级模块化，按需引入。
3. 与 `em_base` 解耦清晰：类型与基础宏来自 `em_base`。

## 模块构成

当前主要包含：

1. 算法工具：`crc`、`filter`、`pid`、`math_util`、`endian_util`。
2. 容器结构：`queue`、`stack`、`lifo`、`singly_list`、`doubly_list`、`doubly_linked_list`。
3. 运行辅助：`soft_timer`、`memory_pool`、`mem_view`、`bitmap`。
