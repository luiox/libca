-- 最基础的库
-- target("libca.base")
--     set_kind("static")
--     add_files("**.cpp")
--     add_includedirs(".", { public = true })
    -- add_includedirs("$(projectdir)/third_party")
    -- add_linkdirs("$(projectdir)/third_party/libiconv/lib")


target("libca.base_test")
    set_kind("binary")
    set_group("core")
    add_files("ByteBuffer.cpp")

    add_files("../test/Test.cpp")

    -- 启用测试
    add_defines("TEST_ENABLE=1")
    -- 使用默认的main函数
    add_defines("TEST_USE_DEFAULT_MAIN=1")
    -- 开启成功的断言信息
    add_defines("TEST_USE_SUCCESS_MSG=1")

    