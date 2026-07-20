# 已弃用：Tensor（张量库）

## 背景

旧 `libca.core/src/old/tensor.hpp`（1709 行）是一份 ML 风格张量运算库：

- 命名空间 `bm`（**不在 `ca::` 体系下**）
- 依赖一个外部 `ts::Tensor<T>` 模板（必须由使用方提供），本身只做算法层
- 提供 `create_with_data` / `rand` / `zeros` / `ones` / `full` / `eye`、
  `concat` / `tile` / `transpose` / `permute` / `view` / `slice`、
  pointwise math（`add/sub/mul/div/log`）、reduce（`sum/mean/max/min`）、
  比较运算、以及完整的 `einsum`（字符串表达式）
- 含一个 `dataHistory` 的梯度/autograd 风格路径，疑似练习/作业代码

## 处置决定

**不迁移**。已随 `libca.core/` 删除。

理由：

1. 命名空间（`bm` / `ts`）与新库 `ca::` 规范不一致，迁移等于重写
2. 依赖外部 `ts::Tensor<T>`，没有自洽的存储层
3. 性质偏练习，不像生产基础设施
4. 与 `libca` 作为"cpp 基础设施库"的定位不符——张量运算更适合独立项目或依赖 Eigen / xtensor

## 未来路径

如果将来确实需要在 `libca` 中引入张量/数值计算：

- 应基于 `ca::` 命名空间从零设计
- 优先考虑包装成熟的第三方库（Eigen、xtensor、libtorch）而非自研
- 作为独立可选模块（如 `libca/numeric` 或 `libca/tensor`），不混入 core
