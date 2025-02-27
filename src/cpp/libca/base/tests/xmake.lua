target("StringTest")
    set_kind("binary")
    add_files("StringTest.cpp")
    add_files("../String.cpp")
    add_includedirs("../")
    