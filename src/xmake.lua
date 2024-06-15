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

target("test-core")
    set_kind("binary")
    add_defines("DEBUG")
    -- doctest main
    add_defines("DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN")

    add_packages("doctest")
    add_packages("trompeloeil")
    add_packages("nanobench")
    add_packages("spdlog")

    add_includedirs("../src")

    add_files("libca/core/test-*.cpp")

    add_deps("ca")
    add_links("ca")

target("test-utility")
    set_kind("binary")
    add_defines("DEBUG")
    -- doctest main
    add_defines("DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN")

    add_packages("doctest")
    add_packages("trompeloeil")
    add_packages("nanobench")
    add_packages("spdlog")

    add_includedirs("../src")

    add_files("libca/utility/test-*.cpp")

    add_deps("ca")
    add_links("ca")


target("test-database")
    set_kind("binary")
    add_defines("DEBUG")
    -- doctest main
    add_defines("DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN")

    add_packages("doctest")
    add_packages("trompeloeil")
    add_packages("nanobench")
    add_packages("spdlog")

    add_includedirs("../src")
    
    add_files("libca/database/test-*.cpp")

    add_deps("ca")
    add_links("ca")

