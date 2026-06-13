add_requires("gtest")

target("libca_core")
    set_kind("static")
    set_group("libs")
    add_headerfiles("src/libca/core/*.hpp")
    add_files("src/libca/core/bytes.cpp")
    add_includedirs("src", {public = true})

target("libca_core_unittest")
    set_kind("binary")
    set_group("libs/test")
    add_packages("gtest")
    add_deps("libca_core")
    add_files("unittest/main.cpp")
    add_files("unittest/*_test.cpp")
    add_includedirs("src")
    set_rundir("$(projectdir)")
    if is_plat("windows") then
        add_cxflags("/utf-8", {tools = "cl"})
    end
