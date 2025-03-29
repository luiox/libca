
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
    -- add_files("libca/base/*.cpp")
    add_files("libca/test/*.cpp")
    add_files("libca/base/Result.cpp")
    add_files("libca/base/BasicValue.cpp")
    add_files("libca/base/ByteBuffer.cpp")
    add_files("libca/base/Wrapper.cpp")
    add_files("libca/reflect/Enum.cpp")
    -- add_files("libca/utility/Ini.cpp")

    add_defines("TEST_USE_DEFAULT_MAIN")
    add_defines("TEST_ENABLE")



