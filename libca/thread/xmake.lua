target("libca_thread")
    set_kind("static")
    set_group("libs")
    add_files("src/libca/thread/*.cpp")
    add_headerfiles("src/(libca/thread/*.hpp)")
    add_includedirs("src", {public = true})
    add_deps("libca_core")

    if is_plat("windows") then
        add_cxflags("/utf-8", {tools = "cl"})
    elseif is_plat("linux") then
        add_syslinks("pthread")
    end

if has_config("with_tests") then
target("libca_thread_unittest")
    set_kind("binary")
    set_default(false)
    add_tests("default")
    set_group("libs/test")
    add_deps("libca_thread")
    add_links("libca_thread", "libca_core")
    add_packages("gtest")
    add_files("unittest/main.cpp")
    add_files("unittest/*_test.cpp")
    add_includedirs("src")
    set_rundir("$(projectdir)")

    if is_plat("windows") then
        add_cxflags("/utf-8", {tools = "cl"})
    end
end
