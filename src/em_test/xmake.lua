-- C的测试库，因为主要是用C测试，所以就归类到嵌入式库算了
target("ca-em_test")
    set_kind("static")
    add_files("**.c")

-- 最小的自测试可执行文件，仅运行测试框架，测试自身是否有问题
target("ca-em_self_test")
    set_kind("binary")

    add_includedirs(".")
    -- 启用测试
    add_defines("TEST_ENABLE=1")
    -- 使用self test自定义的main函数
    add_defines("TEST_SELF_MAIN=1")

    add_files("*.c")
