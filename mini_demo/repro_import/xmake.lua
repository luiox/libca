set_project("mini-demo-repro-import")
set_version("0.0.1")
set_xmakever("2.8.3")
set_languages("c99")

add_rules("mode.debug", "mode.release")

target("repro_import")
    set_kind("binary")
    add_files("main.c")
    on_load(function (target)
        -- 使用 import 导入模块并调用函数封装
        local utils = import("build_utils", {rootdir = os.projectdir()})
        utils.m_add_libs(target, "edriver.led", {port = "board/port_led.c"})
    end)
