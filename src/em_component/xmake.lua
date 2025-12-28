target("test-scoroutine")
    set_kind("binary")
    add_files("scoroutine.c")
    
    add_rules("em_test", { test_enable = true, use_default_main = true })
