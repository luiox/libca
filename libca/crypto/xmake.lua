add_requires("gtest")

target("libca_crypto")
    set_kind("static")
    set_group("libs")
    add_files("src/libca/crypto/*.cpp")
    add_headerfiles("src/libca/crypto/*.hpp")
    add_headerfiles("src/libca/crypto/sha3.h")
    add_includedirs("src", {public = true})

    if is_plat("windows") then
        add_cxflags("/utf-8", {tools = "cl"})
    end

target("libca_crypto_unittest")
    set_kind("binary")
    set_group("libs/test")
    add_deps("libca_crypto")
    add_packages("gtest")
    add_files("unittest/main.cpp")
    add_files("unittest/*_test.cpp")
    add_includedirs("src")
    set_rundir("$(projectdir)")
    if is_plat("windows") then
        add_cxflags("/utf-8", {tools = "cl"})
    end
