target("ca-em_base")
    set_kind("static")
    add_files("**.c")
    add_includedirs(".", { public = true })

