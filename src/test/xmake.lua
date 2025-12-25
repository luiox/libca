-- 特殊的组件库
-- C++的测试库
target("ca-test")
    set_kind("static")
    add_files("*.cpp")

    add_includedirs(".", { public = true })
    -- 启用测试
    add_defines("TEST_ENABLE=1")
    

-- 最小的自测试可执行文件，仅运行测试框架，测试自身是否有问题
target("ca-self_test")
    set_kind("binary")

    add_includedirs(".")
    -- 启用测试
    add_defines("TEST_ENABLE=1")
    -- 使用self test自定义的main函数
    add_defines("TEST_SELF_MAIN=1")

    add_files("*.cpp")
