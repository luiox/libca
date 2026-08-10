-- fmt（header_only）由 libca_str 以 public 依赖提供，log 通过 add_deps("libca_str") 间接拿到。
-- spdlog 后端由根 with_spdlog option 控制（默认 false）。

target("libca_log")
    set_kind("static")
    set_group("libs")
    add_files("src/libca/log/*.cpp")
    add_headerfiles("src/(libca/log/*.hpp)")
    add_headerfiles("src/(libca/log/detail/*.hpp)")
    add_includedirs("src", {public = true})
    add_deps("libca_core", "libca_str")

    if is_plat("windows") then
        add_cxflags("/utf-8", {tools = "cl"})
    end

    -- spdlog 后端：仅 with_spdlog=y 时编译
    if has_config("with_spdlog") then
        add_files("src/libca/log/spdlog/*.cpp")
        add_headerfiles("src/(libca/log/spdlog/*.hpp)")
        add_packages("spdlog", { configs = { header_only = true, fmt_external = true } })
        add_defines("CA_LOG_HAVE_SPDLOG=1")
    end

if has_config("with_tests") then
target("libca_log_unittest")
    set_kind("binary")
    set_default(false)
    add_tests("default")
    set_group("libs/test")
    add_deps("libca_log")
    add_links("libca_log", "libca_core")
    add_packages("gtest")
    add_files("unittest/main.cpp")
    add_files("unittest/*_test.cpp")
    add_includedirs("src")
    set_rundir("$(projectdir)")
    if is_plat("windows") then
        add_cxflags("/utf-8", {tools = "cl"})
    end
    if has_config("with_spdlog") then
        add_packages("spdlog", { configs = { header_only = true, fmt_external = true } })
    else
        -- 无 spdlog 时排除 spdlog_backend_test.cpp（缺 spdlog 头无法编译）
        remove_files("unittest/spdlog_backend_test.cpp")
    end
end

-- 性能基准：独立 main，不依赖 gtest，不注册进 xmake test（不默认构建）。
-- 用 `xmake build libca_log_benchmark && xmake run libca_log_benchmark`。
target("libca_log_benchmark")
    set_kind("binary")
    set_default(false)
    set_group("libs/benchmark")
    add_deps("libca_log")
    add_links("libca_log", "libca_core")
    add_files("benchmark/*.cpp")
    add_includedirs("src")
    set_rundir("$(projectdir)")
    if is_plat("windows") then
        add_cxflags("/utf-8", {tools = "cl"})
    end
    if is_plat("linux") then
        add_syslinks("pthread")
    end
