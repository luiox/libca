target("ca")
    set_kind("shared")
    -- set_kind("$(kind)")
    add_includedirs("../src")

    add_files("libca/core/*.cpp|test-*.cpp")
    add_files("libca/database/*.cpp|test-*.cpp")
    add_files("libca/log/*.cpp|test-*.cpp")
    add_files("libca/utility/*.cpp|test-*.cpp")
    -- set_basename("ca")
    -- add_packages("spdlog")
    add_links("mysqlclient")

target("ca-core")
    set_kind("shared")
    add_includedirs("../src", {public = true})
    add_files("libca/core/*.cpp|test-*.cpp")

target("test-ca-core")
    set_kind("binary")
    add_defines("DEBUG")

    add_packages("doctest")
    
    add_files("libca/test-main.cpp")
    add_files("libca/core/test-*.cpp")
    
    add_deps("ca-core")
    add_links("ca-core")

target("ca-utility")
    set_kind("shared")
    add_includedirs("../src", {public = true})
    add_files("libca/utility/*.cpp|test-*.cpp")

target("test-ca-utility")
    set_kind("binary")
    add_defines("DEBUG")
    -- doctest main
    add_defines("DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN")

    add_packages("doctest")

    add_files("libca/utility/test-*.cpp")
    add_deps("ca-core")
    add_deps("ca-utility")
    add_links("ca-core")
    add_links("ca-utility")

target("ca-database")
    set_kind("shared")
    add_includedirs("../src", {public = true})
    add_files("libca/database/*.cpp|test-*.cpp")
    add_links("mysqlclient")

target("test-ca-database")
    set_kind("binary")
    add_defines("DEBUG")
    -- doctest main
    add_defines("DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN")

    add_packages("doctest")

    add_files("libca/database/test-*.cpp")
    add_deps("ca-core")
    add_deps("ca-database")
    add_links("ca-core")
    add_links("ca-database")

target("ca-log")
    set_kind("shared")
    add_includedirs("../src", {public = true})
    add_files("libca/log/*.cpp|test-*.cpp")

target("test-ca-log")
    set_kind("binary")
    add_defines("DEBUG")
    -- doctest main
    add_defines("DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN")

    add_packages("doctest")

    add_files("libca/log/test-*.cpp")
    add_deps("ca-core")
    add_deps("ca-log")
    add_links("ca-core")
    add_links("ca-log")


