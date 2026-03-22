target("libca.em_motion")
    set_kind("static")
    add_files("*.c")
    add_deps("libca.em_base")