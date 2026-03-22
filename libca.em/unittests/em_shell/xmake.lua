local src_dir = "$(projectdir)/libca.em/src/em_shell"

target("test-shell")
    set_kind("binary")
    set_group("test")
    add_includedirs(src_dir, ".")
    add_files("test_shell.c")
    add_rules("em_test", { test_enable = true, use_default_main = true })
    add_deps("libca.em_base")
