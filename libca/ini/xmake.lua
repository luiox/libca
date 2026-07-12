-- gtest 由根 xmake.lua 的 with_tests option 统一管理（默认 false）。

target("libca_ini")
    set_kind("static")
    set_group("libs")
    add_files("src/libca/ini/*.cpp")
    add_headerfiles("src/libca/ini/*.hpp")
    add_includedirs("src", {public = true})
    add_deps("libca_core")

    if is_plat("windows") then
        add_cxflags("/utf-8", {tools = "cl"})
    end

if has_config("with_tests") then
target("libca_ini_unittest")
    set_kind("binary")
    set_group("libs/test")
    add_deps("libca_ini")
    add_links("libca_ini", "libca_core")
    add_packages("gtest")
    add_files("unittest/main.cpp")
    add_files("unittest/*_test.cpp")
    add_includedirs("src")
    set_rundir("$(projectdir)")
    if is_plat("windows") then
        add_cxflags("/utf-8", {tools = "cl"})
    end
end
