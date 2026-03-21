# demos

新增 import 方案模块化示例。

## 目标

- demo_em_modules: 验证 em_log 可通过 libca.em 注入。
- em_log 支持 backend 字符串选项，默认 simple_logger。

## 命令

在仓库根目录执行:

```bash
xmake f -P demos -m debug
xmake build demo_em_modules
xmake run demo_em_modules
```
