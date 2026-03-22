target("libca.em_protocol")
    set_kind("static")
    set_group("em")
    
    add_files("**.c")
    remove_files("test-*.c")
    add_includedirs(".", { public = true })
    add_deps("libca.em_base")
    add_deps("libca.em_util")

local em_protocol_test_dir = "$(projectdir)/tests/em_protocol"

target("test-xmodem")
    set_kind("binary")
    add_rules("em_test", { test_enable = true, use_default_main = true })
    add_includedirs(".", em_protocol_test_dir)
    add_files("xmodem.c", path.join(em_protocol_test_dir, "test-xmodem-sim.c"))
    add_deps("libca.em_base", "libca.em_util")

target("test-ymodem")
    set_kind("binary")
    add_rules("em_test", { test_enable = true, use_default_main = true })
    add_includedirs(".", em_protocol_test_dir)
    add_files("ymodem.c", path.join(em_protocol_test_dir, "test-ymodem-sim.c"))
    add_deps("libca.em_base", "libca.em_util")

target("test-dummy")
    set_kind("binary")
    add_rules("em_test", { test_enable = true, use_default_main = true })
    add_includedirs(".", em_protocol_test_dir)
    add_files("dummy.c", path.join(em_protocol_test_dir, "test_dummy.c"))
    add_deps("libca.em_base", "libca.em_util")
