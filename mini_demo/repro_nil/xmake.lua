set_project("mini-demo-repro-nil")
set_version("0.0.1")
set_xmakever("2.8.3")
set_languages("c99")

add_rules("mode.debug", "mode.release")

includes("provider.lua")

target("repro_nil")
    set_kind("binary")
    add_files("main.c")
    on_load(function (target)
        -- 预期这里会出现 nil：includes 中定义的函数在该作用域不可见
        m_add_libs(target, "demo.lib", {})
    end)
