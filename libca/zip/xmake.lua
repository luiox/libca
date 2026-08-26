-- gtest 由根 xmake.lua 的 with_tests option 统一管理（默认 false）。
-- 本模块依赖 zlib：由根 with_zip 开关控制是否进入构建（无 zlib 环境整体跳过，
-- 不影响 libca 其余部分）。

target("libca_zip")
    set_kind("static")
    set_group("libs")
    add_packages("zlib")
    add_deps("libca_core")
    add_files("src/libca/zip/*.cpp")
    add_files("src/libca/zip/detail/*.cpp")
    add_headerfiles("src/(libca/zip/*.hpp)")
    add_includedirs("src", {public = true})
    if is_plat("windows") then
        add_cxflags("/utf-8", {tools = "cl"})
    end

if has_config("with_tests") then
target("libca_zip_unittest")
    set_kind("binary")
    set_default(false)
    add_tests("default")
    set_group("libs/test")
    add_deps("libca_zip")
    add_links("libca_zip", "libca_core")
    add_packages("gtest")
    add_packages("zlib")
    add_files("unittest/main.cpp")
    add_files("unittest/*_test.cpp")
    add_includedirs("src")
    set_rundir("$(projectdir)")
    if is_plat("windows") then
        add_cxflags("/utf-8", {tools = "cl"})
    end
end
