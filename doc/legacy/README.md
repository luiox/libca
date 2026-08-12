# doc/legacy

历史遗留内容归档区：不再构建、不再维护，仅保留供查阅/参考。

- `cpp-code-spec.md`：早期 C++ 规范，已被 `prompt/code_rule.md` 取代（权威唯一）。
- `老的em_driver编写规范.md` / `old_readme.md`：早期 em 驱动编写规范与旧 README。
- `utility/`：位操作小工具 `BitsUtil`（原文件名 `BitsUitl`，拼写已修正），未接入构建。
- `reflect/`：早期反射实验代码，未接入构建。

如需复用其中代码，请先评估并迁移到正式模块，再接入 `libca/xmake.lua`。
