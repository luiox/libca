-- gtest 由根 xmake.lua 的 with_tests option 统一管理（默认 false）。

target("libca_str")
    set_kind("static")
    set_group("libs")
    add_files("src/**.cpp")
    add_headerfiles("src/(libca/str/*.hpp)")
    add_includedirs("src", {public = true})
    if is_plat("windows") then
        add_cxflags("/utf-8", {tools = "cl"})
    end
    add_deps("libca_core")

if has_config("with_tests") then
target("libca_str_unittest")
    set_kind("binary")
    set_default(false)
    add_tests("default")
    set_group("libs/test")
    add_packages("gtest")
    add_files("unittest/main.cpp")
    add_files("unittest/*_test.cpp")
    add_includedirs("src")
    add_deps("libca_str")
    add_links("libca_str", "libca_core")
    set_rundir("$(projectdir)")
    if is_plat("windows") then
        add_cxflags("/utf-8", {tools = "cl"})
    end

-- 性能基准：独立 main，不依赖 gtest，不注册进 xmake test（不默认构建）。
-- 用 `xmake build libca_str_benchmark && xmake run libca_str_benchmark`。
target("libca_str_benchmark")
    set_kind("binary")
    set_default(false)
    set_group("libs/benchmark")
    add_deps("libca_str")
    add_links("libca_str", "libca_core")
    add_files("benchmark/*.cpp")
    add_includedirs("src")
    set_rundir("$(projectdir)")
    if is_plat("windows") then
        add_cxflags("/utf-8", {tools = "cl"})
    end
end
