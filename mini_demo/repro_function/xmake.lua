set_project("mini-demo-repro-function")
set_version("0.0.1")
set_xmakever("2.8.3")
set_languages("c99")

add_rules("mode.debug", "mode.release")

includes("provider.lua")

target("repro_function")
    set_kind("binary")
    add_files("main.c")
    -- 描述域直接调用封装函数：这是官方推荐的模块化复用方式
    m_add_libs({
        defines = {"FUNC_WRAP_OK", "FUNC_WRAP_OK_2"}
    })
