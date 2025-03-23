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
    
    



