target("ca")
    -- set_kind("binary")
    set_kind("$(kind)")
    add_includedirs("../src")

    add_files("libca/core/*.cpp|test-*.cpp")
    add_files("libca/database/*.cpp|test-*.cpp")
    add_files("libca/log/*.cpp|test-*.cpp")
    add_files("libca/utility/*.cpp|test-*.cpp")
    -- set_basename("ca")
    -- add_packages("spdlog")
    add_links("mysqlclient")

target("test-utility")
    set_kind("binary")
    add_defines("DEBUG")
    add_includedirs("../src")
    add_deps("ca")
    add_files("libca/utility/test-datetime.cpp")

    add_packages("doctest")
    add_packages("trompeloeil")
    add_packages("nanobench")
    add_packages("spdlog")
    
    -- add_includedirs("third_party")
    -- add_deps("libca-coroutine"
