set_project("mini-demo-repro-rule")
set_version("0.0.1")
set_xmakever("2.8.3")
set_languages("c99")

add_rules("mode.debug", "mode.release")

includes("provider_rule.lua")

target("repro_rule")
    set_kind("binary")
    add_files("main.c")
    add_rules("demo.inject")
