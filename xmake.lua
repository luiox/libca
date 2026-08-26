set_project("libca")
set_version("0.0.1")
set_xmakever("2.8.3")

option("with_core")
    set_default(true)
    set_showmenu(true)
    set_description("Enable libca (C++ core) targets")
option_end()

option("with_em")
    set_default(true)
    set_showmenu(true)
    set_description("Enable libca.em targets")
option_end()

option("with_demo")
    set_default(true)
    set_showmenu(true)
    set_description("Enable demo targets")
option_end()

-- 测试开关：控制是否拉取 gtest 并启用 *_unittest target。
-- 默认 false：libca 独立构建跑测试需 `xmake f --with_tests=y`。
-- 作为 submodule 被 includes 时（如下游仓库直接 includes 子库 xmake.lua），
-- 默认不构建测试，避免强制拉取 gtest / 强制定义 *_unittest target。
option("with_tests")
    set_default(false)
    set_showmenu(true)
    set_description("Enable *_unittest targets and pull gtest via xrepo.")
option_end()

option("with_openssl")
    set_default(false)
    set_showmenu(true)
    set_description("Enable optional OpenSSL HTTPS client support")
option_end()

option("with_spdlog")
    set_default(false)
    set_showmenu(true)
    set_description("Enable optional spdlog backend for libca.log")
option_end()

if has_config("with_spdlog") then
    add_requires("spdlog", { configs = { header_only = true, fmt_external = true } })
end

if has_config("with_tests") then
    add_requires("gtest")
end

if has_config("with_openssl") then
    add_requires("openssl3")
end

if is_plat("windows") then
    -- Xmake maps c99 to /TP for MSVC; C11 keeps em sources in C mode.
    set_languages("c11", "cxx17")
else
    set_languages("c99", "cxx17")
end

add_rules("mode.debug", "mode.release", "mode.coverage")
add_rules("plugin.compile_commands.autoupdate", {outputdir = "."})
-- 如果上面这个不行使用下面这个命令
-- xmake project -k compile_commands

-- 设置 C 编译选项
-- add_cflags("-finput-charset=UTF-8", "-fexec-charset=UTF-8")

-- 设置 C++ 编译选项
-- add_cxflags("-finput-charset=UTF-8", "-fexec-charset=UTF-8")

-- 如果是 msvc 编译器，则使用以下选项
if is_plat("windows") then
    -- 仅对 MSVC 生效；gcc/clang（含 MinGW）源码默认按 UTF-8 处理，无需该选项。
    add_cflags("/utf-8", {tools = "cl"})
    add_cxflags("/utf-8", {tools = "cl"})
end

-- add_requires("doctest 2.4.11")
-- add_requires("trompeloeil 47")
-- add_requires("nanobench 4.3.11")
-- add_requires("spdlog 1.14.1")

on_load(function (target)
    -- 检查当前是否是 coverage 模式
    if is_mode("coverage") then
        -- 仅为 gcc/clang 添加 gcov 标志；跳过 MSVC（Windows 的覆盖率工具不同）
        if is_plat("windows") then
            return
        end

        -- 添加编译/链接标志以生成 gcno/gcda
        target:add("cflags", "-fprofile-arcs", "-ftest-coverage", "-fno-omit-frame-pointer")
        target:add("cxxflags", "-fprofile-arcs", "-ftest-coverage", "-fno-omit-frame-pointer")
        target:add("ldflags", "-fprofile-arcs", "-ftest-coverage", {force = true})

        -- 减少内联以提高覆盖率准确性，并保留调试符号
        target:add("cxflags", "-fno-inline")
        target:set("symbols", "debug")
        target:add("defines", "COVERAGE_BUILD")
    end
end)

includes("xmake/modules/libca/tool/logger.lua")

if has_config("with_em") then
    includes("libca.em")
end

if has_config("with_core") then
    includes("libca")
end
