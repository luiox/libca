target("libca.em_base")
    set_kind("object")
    set_group("em")
    add_files("**.c")

target("libca.em_base_static")
    set_kind("static")
    set_group("em")
    add_files("**.c")


