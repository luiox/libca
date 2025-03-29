
option("network")
    set_default(false)
    set_showmenu(true)
    set_description("Enable network support")
option_end()

target("ca-shared")
    set_kind("shared")
    add_includedirs(".")
    add_files("libca/base/*.cpp")
    add_files("libca/test/*.cpp")
    add_files("libca/reflect/*.cpp")
    add_files("libca/utility/*.cpp")
    
    add_defines("LIBCA_DLL_MODE")
    add_defines("LIBCA_DLL_EXPORT")

    add_includedirs("$(projectdir)/third_party")
    add_files("$(projectdir)/third_party/zlib/*.c")
    add_files("$(projectdir)/third_party/minizip/*.cpp")
    add_files("$(projectdir)/third_party/zip_utils/*.cpp")

    if has_config("network") then
        add_defines("USE_LIBCA_NETWORK=1")
        add_files("libca/network/*.cpp")
    end

target("ca-static")
    set_kind("static")
    add_includedirs(".")
    add_files("libca/base/*.cpp")
    add_files("libca/test/*.cpp")
    add_files("libca/reflect/*.cpp")
    add_files("libca/utility/*.cpp")
    
    if has_config("network") then
        add_defines("USE_LIBCA_NETWORK=1")
        add_files("libca/network/*.cpp")
    end

target("ca-test")
    set_kind("binary")
    add_includedirs(".")
    add_files("libca/base/*.cpp")
    add_files("libca/test/*.cpp")
    add_files("libca/reflect/*.cpp")
    add_files("libca/utility/*.cpp")

    if has_config("network") then
        add_defines("USE_LIBCA_NETWORK=1")
        add_files("libca/network/*.cpp")
    end

    add_defines("TEST_USE_DEFAULT_MAIN")
    add_defines("TEST_ENABLE")



