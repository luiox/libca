local em_component_test_dir = "$(projectdir)/tests/em_component"

target("test-scoroutine")
    set_kind("binary")
    add_includedirs(".", em_component_test_dir)
    add_files("scoroutine.c", path.join(em_component_test_dir, "test_scoroutine.c"))
    
    add_rules("em_test", { test_enable = true, use_default_main = true })

target("test-skv")
    set_kind("binary")
    add_rules("em_test", { test_enable = true, use_default_main = true })
    add_includedirs(".", em_component_test_dir)
    add_files("skv.c", path.join(em_component_test_dir, "test_skv.c"))
    add_deps("libca.em_base")
    add_deps("libca.em_util")

target("test-ini_util")
    set_kind("binary")
    add_rules("em_test", { test_enable = true, use_default_main = true })
    add_includedirs(".", em_component_test_dir)
    add_files("ini.c", path.join(em_component_test_dir, "test_ini.c"))
    add_deps("libca.em_base")