local src_dir = "$(projectdir)/libca.em/src/em_protocol"

target("test-xmodem")
    set_kind("binary")
    add_includedirs(src_dir, ".")
    add_files(path.join(src_dir, "xmodem.c"), "test-xmodem-sim.c")
    add_rules("em_test", { test_enable = true, use_default_main = true })
    add_deps("libca.em_base", "libca.em_util")

target("test-ymodem")
    set_kind("binary")
    add_includedirs(src_dir, ".")
    add_files(path.join(src_dir, "ymodem.c"), "test-ymodem-sim.c")
    add_rules("em_test", { test_enable = true, use_default_main = true })
    add_deps("libca.em_base", "libca.em_util")

target("test-dummy")
    set_kind("binary")
    add_includedirs(src_dir, ".")
    add_files(path.join(src_dir, "dummy.c"), "test_dummy.c")
    add_rules("em_test", { test_enable = true, use_default_main = true })
    add_deps("libca.em_base", "libca.em_util")
