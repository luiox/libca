-- gtest 由根 xmake.lua 的 with_tests option 统一管理（默认 false）。

-- headeronly: Stream 和 ImmutableList 均为模板类，必须在头文件中实现
target("libca_collection")
    set_kind("headeronly")
    set_group("libs")
    add_headerfiles("src/libca/collection/*.hpp")
    add_includedirs("src", {public = true})

if has_config("with_tests") then
target("libca_collection_unittest")
    set_kind("binary")
    set_group("libs/test")
    add_packages("gtest")
    add_files("unittest/main.cpp")
    add_files("unittest/*_test.cpp")
    add_includedirs("src")
    set_rundir("$(projectdir)")
    if is_plat("windows") then
        add_cxflags("/utf-8", {tools = "cl"})
    end
end
