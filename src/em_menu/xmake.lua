target("em_menu")
    set_kind("static")
    add_files("menu.c")
    add_deps("em_base")

target("test-em_menu")
    set_kind("binary")
    add_files("menu_runner.c")
    add_deps("em_menu")
