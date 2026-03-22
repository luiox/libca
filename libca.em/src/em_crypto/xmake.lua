target("libca.em_crypto")
    set_kind("static")
    add_files("*.c")
    add_deps("libca.em_base")
