-- gtest 由根 xmake.lua 的 with_tests option 统一管理（默认 true）。

target("libca_time")
    set_kind("static")
    set_group("libs")
    add_files("src/libca/time/*.cpp")
    add_headerfiles("src/libca/time/*.hpp")
    add_includedirs("src", {public = true})

    if is_plat("windows") then
        add_cxflags("/utf-8", {tools = "cl"})
    end

target("libca_time_unittest")
    set_kind("binary")
    set_group("libs/test")
    add_deps("libca_time")
    add_packages("gtest")
    add_files("unittest/main.cpp")
    add_files("unittest/*_test.cpp")
    add_includedirs("src")
    set_rundir("$(projectdir)")
    if is_plat("windows") then
        add_cxflags("/utf-8", {tools = "cl"})
    end
