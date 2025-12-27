target("ca-em_base")
    set_kind("static")
    add_files("**.c")
    add_includedirs(".", { public = true })

-- ringbuffer的单元测试
-- 运行
-- xmake run ca-em_rb_test
target("ca-em_rb_test")
    set_kind("binary")
    add_files("ringbuffer.c")
    add_files("../em_test/test.c")
    add_includedirs(".", "../em_test")
    -- 启用测试
    add_defines("TEST_ENABLE=1")
    -- 使用self test自定义的main函数
    add_defines("TEST_SELF_MAIN=1")
