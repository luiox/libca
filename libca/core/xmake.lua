-- gtest 由根 xmake.lua 的 with_tests option 统一管理（默认 false）。
-- *_unittest target 受 has_config("with_tests") 守卫，关闭时不定义。

target("libca_core")
    set_kind("static")
    set_group("libs")
    add_headerfiles("src/(libca/core/*.hpp)")
    add_files("src/libca/core/bytes.cpp")
    add_files("src/libca/core/dynamic_library.cpp")
    add_includedirs("src", {public = true})

    if is_plat("linux") then
        add_syslinks("dl")
    end

if has_config("with_tests") then
target("libca_core_unittest")
    set_kind("binary")
    set_default(false)
    add_tests("default")
    set_group("libs/test")
    add_deps("libca_core")
    add_links("libca_core")
    add_packages("gtest")
    add_files("unittest/main.cpp")
    add_files("unittest/*_test.cpp")
    add_includedirs("src")
    set_rundir("$(projectdir)")
    if is_plat("windows") then
        add_cxflags("/utf-8", {tools = "cl"})
    end
end

-- 性能基准：独立 main，不依赖 gtest，不注册进 xmake test（不默认构建）。
-- 用 `xmake build libca_core_benchmark && xmake run libca_core_benchmark`。
target("libca_core_benchmark")
    set_kind("binary")
    set_default(false)
    set_group("libs/benchmark")
    add_deps("libca_core")
    add_links("libca_core")
    add_files("benchmark/*.cpp")
    add_includedirs("src")
    set_rundir("$(projectdir)")
    if is_plat("windows") then
        add_cxflags("/utf-8", {tools = "cl"})
    end
