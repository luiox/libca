local src_dir = "$(projectdir)/libca.em/src/em_component"

target("test-scoroutine")
    set_kind("binary")
    add_includedirs(src_dir, ".")
    add_files(path.join(src_dir, "scoroutine.c"), "test_scoroutine.c")
    add_rules("em_test", { test_enable = true, use_default_main = true })

target("test-skv")
    set_kind("binary")
    add_includedirs(src_dir, ".")
    add_files(path.join(src_dir, "skv.c"), "test_skv.c")
    add_rules("em_test", { test_enable = true, use_default_main = true })
    add_deps("libca.em_base", "libca.em_util")

target("test-ini_util")
    set_kind("binary")
    add_includedirs(src_dir, ".")
    add_files(path.join(src_dir, "ini.c"), "test_ini.c")
    add_rules("em_test", { test_enable = true, use_default_main = true })
    add_deps("libca.em_base")
