target("libca.em_bus")
    set_kind("static")
    add_files("**.c")
    add_deps("libca.em_base")