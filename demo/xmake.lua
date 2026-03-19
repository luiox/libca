set_project("libca-demo")
set_version("0.0.1")
set_xmakever("2.8.3")
set_languages("c99")

add_rules("mode.debug", "mode.release")

-- 引入 libca 的 em 规则与 m_add_libs 接口
includes("../libca_em.lua")

target("demo_led_extern")
    set_kind("binary")
    add_files("app/main.c", "app/debug_stub.c")
    add_rules("libca.em_driver.led", {
        mode = "extern",
        port = {"board/port_led.c"}
    })

target("demo_led_dynamic")
    set_kind("binary")
    add_files("app/main_dynamic.c", "app/debug_stub.c")
    add_rules("libca.em_driver.led", {mode = "dynamic"})

-- 说明：
-- 在当前 xmake 作用域模型下，从 includes("../libca_em.lua") 导出的函数
-- （例如 m_add_libs）不会自动暴露到本文件作用域，因此外部工程
-- 直接调用 m_add_libs(target, ...) 会报 nil。
