target("libca_io")
    set_kind("static")
    set_group("libs")
    add_files("src/libca/io/*.cpp")
    add_headerfiles("src/(libca/io/*.hpp)")
    add_includedirs("src", {public = true})
    add_deps("libca_core")

    if is_plat("windows") then
        add_cxflags("/utf-8", {tools = "cl"})
    end

if has_config("with_tests") then
target("libca_io_unittest")
    set_kind("binary")
    set_default(false)
    set_group("libs/test")
    add_deps("libca_io")
    add_links("libca_io", "libca_core")
    add_packages("gtest")
    add_files("unittest/main.cpp")
    add_files("unittest/*_test.cpp")
    add_includedirs("src")
    set_rundir("$(projectdir)")

    if is_plat("windows") then
        add_cxflags("/utf-8", {tools = "cl"})
    end
end
