-- 门面宏依赖 fmt（header_only）。spdlog 后端由根 with_spdlog option 控制（默认 false）。
add_requires("fmt", { configs = { header_only = true } })

target("libca_log")
    set_kind("static")
    set_group("libs")
    add_files("src/libca/log/*.cpp")
    add_headerfiles("src/(libca/log/*.hpp)")
    add_headerfiles("src/(libca/log/detail/*.hpp)")
    add_includedirs("src", {public = true})
    add_deps("libca_core")
    add_packages("fmt", {public = true})

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
    end
end
