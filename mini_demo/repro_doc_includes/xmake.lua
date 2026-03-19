set_project("mini-demo-doc-includes")
set_version("0.0.1")
set_xmakever("2.8.3")
set_languages("c99")

add_rules("mode.debug", "mode.release")

includes("modules/provider.lua")

target("repro_doc_includes")
    set_kind("binary")
    add_files("src/main.c")
    my_add_libs("led")
