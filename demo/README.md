# demo

该目录用于验证外部工程通过 `libca_em.lua` 接入 `em_driver` 的可行方案。

## 构建命令

在仓库根目录执行：

```bash
xmake -P demo
xmake -P demo demo_led_extern
xmake -P demo demo_led_dynamic
```

## 关键点

- 只需 includes("../libca_em.lua") 一次。
- 在当前 xmake 作用域下，include 文件中的函数不会稳定暴露到外层工程。
- 因此外部工程推荐使用 add_rules("libca.em_driver.led", opts) 方式接入。
- extern 模式可通过 port 列表注入用户适配源码。
- dynamic 模式下不注入 port 文件，由用户在代码中调用 led_bind_port()。
- demo 中通过 app/debug_stub.c 提供最小 debug_printf 实现，只用于验证接入链路。
