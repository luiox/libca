
target("test")
    set_kind("binary")
    add_defines("DEBUG")
    add_includedirs("include")
    add_includedirs("test")
    add_deps("libca")
    add_files("test/**.cpp")
    if is_plat("linux", "macosx") then
        add_links("pthread", "m", "dl")
    end
    add_packages("doctest")
    add_packages("FakeIt")
    add_packages("spdlog")
    
    -- add_includedirs("third_party")
    -- add_deps("libca-coroutine"
target_end()
