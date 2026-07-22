# demo

该目录用于验证外部工程通过 `import("libca.em")` 接入源码包管理方案。

## 构建命令

在仓库根目录执行：

```bash
xmake f -P libca.em/demo -c -y
xmake build -P libca.em/demo -a
xmake run -P libca.em/demo demo_led_extern
xmake run -P libca.em/demo demo_led_dynamic
xmake run -P libca.em/demo demo_led_no_port
xmake run -P libca.em/demo demo_module_batch
xmake run -P libca.em/demo demo_driver_manifests_check
xmake run -P libca.em/demo check_em_contract
```

## 关键点

- 仅需 `add_moduledirs(...)` 并在 `on_load` 使用 `import("libca.em")`。
- 通过 `em.setup(target, {root = ...})` 初始化源码根目录。
- 通过 `em.add_libs(target, {em_xxx = opts})` 显式添加模块与依赖，table内顺序无关。
- `check_em_contract`验证完整module清单、依赖、去重、重复配置和错误诊断。
- extern 模式可通过 port 列表注入用户适配源码。
- 未提供 port 时解释器不注入任何 port 文件。
- dynamic 模式下由用户在代码中调用 led_bind_port()。
