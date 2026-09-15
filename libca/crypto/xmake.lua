-- gtest 由根 xmake.lua 的 with_tests option 统一管理（默认 false）。

target("libca_crypto")
    set_kind("static")
    set_group("libs")
    add_files("src/libca/crypto/*.cpp")
    add_headerfiles("src/(libca/crypto/*.hpp)")
    add_headerfiles("src/(libca/crypto/sha3.h)")
    add_includedirs("src", {public = true})
    add_deps("libca_core")

    if has_config("with_openssl") then
        add_defines("LIBCA_CRYPTO_HAS_OPENSSL")
        add_packages("openssl3", {public = true})
    end

    if is_plat("windows", "mingw") then
        add_cxflags("/utf-8", {tools = "cl"})
        add_syslinks("bcrypt")
    end

if has_config("with_tests") then
target("libca_crypto_unittest")
    set_kind("binary")
    set_default(false)
    add_tests("default")
    set_group("libs/test")
    add_deps("libca_crypto")
    add_links("libca_crypto", "libca_core")
    add_packages("gtest")
    add_files("unittest/main.cpp")
    add_files("unittest/*_test.cpp")
    add_includedirs("src")
    set_rundir("$(projectdir)")

    if has_config("with_openssl") then
        add_defines("LIBCA_CRYPTO_HAS_OPENSSL")
    end

    if is_plat("windows", "mingw") then
        add_cxflags("/utf-8", {tools = "cl"})
    end
end

-- 加密性能基准（手动运行：xmake build -P . libca_crypto_perf）。
target("libca_crypto_perf")
    set_kind("binary")
    set_default(false)
    set_group("libs/perf")
    add_deps("libca_crypto", "libca_time")
    add_files("perf/crypto_perf.cpp")
    add_includedirs("src")

    if has_config("with_openssl") then
        add_defines("LIBCA_CRYPTO_HAS_OPENSSL")
    end

    if is_plat("windows", "mingw") then
        add_cxflags("/utf-8", {tools = "cl"})
    end
