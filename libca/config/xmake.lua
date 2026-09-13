-- gtest 由根 xmake.lua 的 with_tests option 统一管理（默认 false）。

target("libca_config")
    set_kind("static")
    set_group("libs")
    add_files("src/libca/config/*.cpp")
    add_headerfiles("src/(libca/config/*.hpp)")
    add_includedirs("src", {public = true})
    -- 上层模块：单向依赖 core(L0) / json / fs；json 的 str 头经其 public includedirs 传递
    add_deps("libca_core", "libca_json", "libca_fs")

    if is_plat("windows", "mingw") then
        add_cxflags("/utf-8", {tools = "cl"})
    end

if has_config("with_tests") then
target("libca_config_unittest")
    set_kind("binary")
    set_default(false)
    add_tests("default")
    set_group("libs/test")
    add_deps("libca_config")
    add_links("libca_config", "libca_json", "libca_fs", "libca_str", "libca_core")
    add_packages("gtest")
    add_files("unittest/main.cpp")
    add_files("unittest/*_test.cpp")
    add_includedirs("src")
    set_rundir("$(projectdir)")
    if is_plat("windows", "mingw") then
        add_cxflags("/utf-8", {tools = "cl"})
    end
end
