# demo

该目录用于验证外部工程通过 `import("libca.em")` 接入源码包管理方案。

## 构建命令

在仓库根目录执行：

```bash
xmake -P demo
xmake -P demo build demo_led_extern
xmake -P demo run demo_led_extern
xmake -P demo build demo_led_dynamic
xmake -P demo run demo_led_dynamic
xmake -P demo build demo_led_no_port
xmake -P demo run demo_led_no_port
xmake -P demo build demo_driver_manifests_check
xmake -P demo run demo_driver_manifests_check
```

## 关键点

- 仅需 `add_moduledirs(...)` 并在 `on_load` 使用 `import("libca.em")`。
- 通过 `em.setup(target, {root = ...})` 初始化源码根目录。
- 通过 `em.add_libs(target, "em_xxx", opts)` 显式添加模块与依赖。
- extern 模式可通过 port 列表注入用户适配源码。
- 未提供 port 时解释器不注入任何 port 文件。
- dynamic 模式下由用户在代码中调用 led_bind_port()。
