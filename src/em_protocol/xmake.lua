target("libca.em_protocol")
    set_kind("static")
    set_group("em")
    
    add_files("**.c")
    remove_files("test-*.c")
    add_includedirs(".", { public = true })
    add_deps("libca.em_base")
    add_deps("libca.em_util")
