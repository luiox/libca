target("test-datetime")
    set_kind("binary")
    add_defines("DEBUG")
    add_includedirs("../../include")
    add_includedirs("../")
    add_deps("ca")
    add_files("**.cpp")
    if is_plat("linux", "macosx") then
        add_links("pthread", "m", "dl")
    end
    add_packages("doctest")
    add_packages("trompeloeil")
    add_packages("nanobench")
    add_packages("spdlog")
    
    -- add_includedirs("third_party")
    -- add_deps("libca-coroutine"
target_end()
