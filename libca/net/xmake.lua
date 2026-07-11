target("libca_net")
    set_kind("static")
    set_group("libs")
    add_files("src/libca/net/*.cpp")
    add_headerfiles("src/libca/net/*.hpp")
    add_includedirs("src", {public = true})
    add_deps("libca_io")

    if is_plat("windows") then
        add_syslinks("ws2_32")
        add_cxflags("/utf-8", {tools = "cl"})
    end

if has_config("with_tests") then
target("libca_net_unittest")
    set_kind("binary")
    set_group("libs/test")
    add_deps("libca_net")
    add_packages("gtest")
    add_files("unittest/main.cpp")
    add_files("unittest/*_test.cpp")
    add_includedirs("src")
    set_rundir("$(projectdir)")

    if is_plat("windows") then
        add_cxflags("/utf-8", {tools = "cl"})
    end
end
