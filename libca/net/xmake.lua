target("libca_net")
    set_kind("static")
    set_group("libca")
    add_files("src/libca/net/*.cpp")
    add_headerfiles("src/(libca/net/*.hpp)")
    add_includedirs("src", {public = true})
    add_deps("libca_io", "libca_str", "libca_collection")

    if has_config("with_openssl") then
        add_defines("LIBCA_NET_HAS_OPENSSL")
        add_packages("openssl3", {public = true})
    end

    if is_plat("windows", "mingw") then
        -- iphlpapi 为 sock_util.cpp 的 interface_list 所需（GetAdaptersAddresses，
        -- 50bb642 引入）。静态库 target 的 add_syslinks 不随包安装传导：消费方经
        -- luiox-repo 包消费时，系统库须由包定义的 add_syslinks 清单补齐，此处新增
        -- 系统库依赖时须同步该清单，否则包消费方链接报 LNK2019 __imp_*
        --（morpher#952 实证）。
        add_syslinks("ws2_32", "iphlpapi")
        add_cxflags("/utf-8", {tools = "cl"})
    end

if has_config("with_tests") then
target("libca_net_unittest")
    set_kind("binary")
    set_default(false)
    add_tests("default")
    set_group("libca/test")
    add_deps("libca_net")
    add_links("libca_net", "libca_io", "libca_core", "libca_str")
    add_packages("gtest")
    add_files("unittest/main.cpp")
    add_files("unittest/*_test.cpp")
    add_includedirs("src", "test")
    set_rundir("$(projectdir)")

    if has_config("with_openssl") then
        add_defines("LIBCA_NET_HAS_OPENSSL")
    end

    if is_plat("windows", "mingw") then
        add_cxflags("/utf-8", {tools = "cl"})
    end
end

-- 性能基准（不参与默认构建与 add_tests）
-- 用法：xmake build -P . libca_net_perf && 直接运行产物。
target("libca_net_perf")
    set_kind("binary")
    set_default(false)
    set_group("libca/perf")
    add_deps("libca_net", "libca_time")
    add_links("libca_net", "libca_io", "libca_core", "libca_str", "libca_time")
    add_files("perf/net_perf.cpp")
    add_includedirs("src", "test")
    set_rundir("$(projectdir)")

    if has_config("with_openssl") then
        add_defines("LIBCA_NET_HAS_OPENSSL")
    end

    if is_plat("windows", "mingw") then
        add_cxflags("/utf-8", {tools = "cl"})
    end

target("libca_net_sock_perf")
    set_kind("binary")
    set_default(false)
    set_group("libca/perf")
    add_deps("libca_net", "libca_time")
    add_links("libca_net", "libca_io", "libca_core", "libca_str", "libca_time")
    add_files("perf/sock_perf.cpp")
    add_includedirs("src")

    if is_plat("windows", "mingw") then
        add_cxflags("/utf-8", {tools = "cl"})
    end
