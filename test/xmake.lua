target("test-ping_pong_buffer")
    set_kind("binary")
    add_deps("libca-em")
    add_files("ping_pong_buffer-test.c")

target("test-ini")
    set_kind("binary")
    add_deps("libca-em")
    add_files("test-ini.c")

target("test-skv")
    set_kind("binary")
    add_deps("libca-em")
    add_files("test-skv.c")

target("test-co")
    set_kind("binary")
    -- add_deps("libca-em")
    add_files("test-co.c")

target("test-sco")
    set_kind("binary")
    add_deps("libca-em")
    add_files("test-scoroutine.c")

target("test-log")
    set_kind("binary")
    add_deps("libca-em")
    add_files("test-log.c")
