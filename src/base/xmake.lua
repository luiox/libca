-- 最基础的库
target("ca-base")
    set_kind("static")
    add_files("**.cpp")
    add_includedirs(".", { public = true })
    -- add_includedirs("$(projectdir)/third_party")
    -- add_linkdirs("$(projectdir)/third_party/libiconv/lib")


target("ca-base_test")
    set_kind("binary")
    add_files("ByteBuffer.cpp")

    add_files("../test/Test.cpp")

    -- 启用测试
    add_defines("TEST_ENABLE=1")
    -- 使用默认的main函数
    add_defines("TEST_USE_DEFAULT_MAIN=1")
    