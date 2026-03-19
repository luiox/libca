-- 仅仅用于内部测试
target("em_driver.led.extern")
    set_kind("static")
    set_default("false")
    add_rules("libca.em_driver.led", {mode = "extern"})

