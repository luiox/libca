-- libca_ui：Windows-only 子库。其它平台不定义任何 target，
-- 因此跨平台项目可无条件 includes("ui") 而不会在 Linux 上失败。

if is_plat("windows") then
target("libca_ui")
    set_kind("static")
    set_group("libs")
    add_files("src/libca/ui/*.cpp")
    add_headerfiles("src/(libca/ui/*.hpp)")
    add_includedirs("src", {public = true})
    add_deps("libca_core", "libca_str")
    add_cxflags("/utf-8", {tools = "cl"})
    -- Win32 GUI 与窗口管理必需的系统库。
    add_syslinks("user32", "gdi32")
end

if has_config("with_tests") and is_plat("windows") then
target("libca_ui_unittest")
    set_kind("binary")
    set_default(false)
    add_tests("default")
    set_group("libs/test")
    add_deps("libca_ui")
    add_links("libca_ui", "libca_str", "libca_core")
    add_packages("gtest")
    add_files("unittest/main.cpp")
    add_files("unittest/*_test.cpp")
    add_includedirs("src")
    set_rundir("$(projectdir)")
    add_cxflags("/utf-8", {tools = "cl"})
    add_syslinks("user32", "gdi32")
end
