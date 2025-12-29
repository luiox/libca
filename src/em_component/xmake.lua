target("test-scoroutine")
    set_kind("binary")
    add_files("scoroutine.c")
    
    add_rules("em_test", { test_enable = true, use_default_main = true })

target("test-skv_util")
    set_kind("binary")
    add_rules("em_test", { test_enable = true, use_default_main = true })
    add_files("skv.c")
    add_deps("ca-em_base")

target("test-ini_util")
    set_kind("binary")
    add_rules("em_test", { test_enable = true, use_default_main = true })
    add_files("ini.c")
    add_deps("ca-em_base")
