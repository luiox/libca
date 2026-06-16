-- gtest 由根 xmake.lua 的 with_tests option 统一管理（默认 false）。

target("libca_str")
    set_kind("static")
    set_group("libs")
    add_files("src/**.cpp")
    add_includedirs("src", {public = true})
    if is_plat("windows") then
        add_cxflags("/utf-8", {tools = "cl"})
    end
    add_deps("libca_core")

if has_config("with_tests") then
target("libca_str_unittest")
    set_kind("binary")
    set_group("libs/test")
    add_packages("gtest")
    add_files("unittest/main.cpp")
    add_files("unittest/*_test.cpp")
    add_includedirs("src")
    add_deps("libca_str")
    set_rundir("$(projectdir)")
    if is_plat("windows") then
        add_cxflags("/utf-8", {tools = "cl"})
    end
end
