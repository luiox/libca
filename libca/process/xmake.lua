target("libca_process")
    set_kind("static")
    set_group("libs")
    add_files("src/libca/process/*.cpp")
    add_headerfiles("src/libca/process/*.hpp")
    add_includedirs("src", {public = true})
    add_deps("libca_core")

    if is_plat("windows") then
        add_cxflags("/utf-8", {tools = "cl"})
    elseif is_plat("linux") then
        add_syslinks("pthread")
    end

if has_config("with_tests") then
target("libca_process_unittest")
    set_kind("binary")
    set_group("libs/test")
    add_deps("libca_process")
    add_packages("gtest")
    add_files("unittest/main.cpp")
    add_files("unittest/*_test.cpp")
    add_includedirs("src")
    set_rundir("$(projectdir)")

    if is_plat("windows") then
        add_cxflags("/utf-8", {tools = "cl"})
    end
end
