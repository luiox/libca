add_requires("gtest")

target("libca_str")
    set_kind("static")
    set_group("libs")
    add_files("src/**.cpp")
    add_includedirs("src", {public = true})
    -- 依赖 libca/core 的基础类型 (datatype.hpp)
    add_includedirs("$(projectdir)/libca/core/src", {public = true})
    if is_plat("windows") then
        add_cxflags("/utf-8", {tools = "cl"})
    end

target("libca_str_unittest")
    set_kind("binary")
    set_group("libs/test")
    add_packages("gtest")
    add_files("unittest/main.cpp")
    add_files("unittest/*_test.cpp")
    add_includedirs("src")
    add_includedirs("$(projectdir)/libca/core/src")
    add_deps("libca_str")
    set_rundir("$(projectdir)")
    if is_plat("windows") then
        add_cxflags("/utf-8", {tools = "cl"})
    end
