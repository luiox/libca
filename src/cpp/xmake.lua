includes("libca/base/tests/*.lua")

option("network")
    set_default(false)
    set_showmenu(true)
    set_description("Enable network support")
option_end()

target("ca")
    set_kind("static")
    add_includedirs(".")
    add_files("libca/base/*.cpp")

    if has_config("network") then
        add_defines("USE_LIBCA_NETWORK=1")
        add_files("libca/network/*.cpp")
    end

target("ca-test")
    set_kind("binary")
    add_includedirs(".")
    add_files("libca/base/*.cpp")
    add_files("libca/test/*.cpp")

    add_defines("TEST_USE_DEFAULT_MAIN")
    add_defines("TEST_ENABLE")



