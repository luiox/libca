target("libca.em_shell")
    set_kind("object")
    set_group("em")
    add_files("shell.c")
    add_deps("libca.em_base")

target("libca.em_shell_static")
    set_kind("static")
    set_group("em")
    add_files("shell.c")
    add_deps("libca.em_base")

local em_shell_test_dir = "$(projectdir)/tests/em_shell"

target("test-shell")
    set_kind("binary")
    set_group("test")
    add_includedirs(".", em_shell_test_dir)
    add_files(path.join(em_shell_test_dir, "test_shell.c"))
    add_rules("em_test", { test_enable = true, use_default_main = true })
    add_deps("libca.em_base")

target("shell_example")
    set_kind("binary")
    set_group("example")
    add_files("example.c")
    add_deps("libca.em_shell")
