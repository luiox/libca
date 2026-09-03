-- gtest 由根 xmake.lua 的 with_tests option 统一管理（默认 false）。

target("libca_xml")
    set_kind("static")
    set_group("libs")
    add_files("src/libca/xml/*.cpp")
    add_headerfiles("src/(libca/xml/*.hpp)")
    add_includedirs("src", {public = true})
    add_deps("libca_core", "libca_str", "libca_fs")

    if is_plat("windows") then
        add_cxflags("/utf-8", {tools = "cl"})
    end

if has_config("with_tests") then
target("libca_xml_unittest")
    set_kind("binary")
    set_default(false)
    add_tests("default")
    set_group("libs/test")
    add_deps("libca_xml")
    add_links("libca_xml", "libca_str", "libca_core")
    add_packages("gtest")
    add_files("unittest/main.cpp")
    add_files("unittest/*_test.cpp")
    add_includedirs("src")
    set_rundir("$(projectdir)")
    if is_plat("windows") then
        add_cxflags("/utf-8", {tools = "cl"})
    end
end
