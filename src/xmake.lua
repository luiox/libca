-- 最基础的库
target("ca-base")
    set_kind("static")
    add_files("base/**.c")
    add_includedirs(".", { public = true })

-------------------------------------------------------------------------------
-- 特殊的组件库
-- C的测试库，因为主要是用C测试，所以就归类到嵌入式库算了
target("ca-em_test")
    set_kind("static")
    add_files("em_test/**.c")
    add_deps("ca-base")

-- C++的测试库
target("ca-test")
    set_kind("static")
    -- add_files("em_test/**.c")
    add_deps("ca-base")

-------------------------------------------------------------------------------
-- 嵌入式库
-- 嵌入式实用工具库
target("ca-em_util")
    set_kind("static")
    add_files("em_util/**.c")
    add_deps("ca-base")

-- 嵌入式驱动库
target("ca-em_driver")
    set_kind("static")
    add_files("em_driver/**.c")
    add_deps("ca-base")

-- 嵌入式日志库
target("ca-em_log")
    set_kind("static")
    add_files("em_log/**.c")
    add_deps("ca-base")  

-- 嵌入式容器库
target("ca-em_collection")
    set_kind("static")
    add_files("em_collection/**.c")
    remove_files("em_collection/test-*.c")
    add_deps("ca-base")

-- target("ca-c-container-test")
--     set_kind("binary")
--     add_files("test-*.c")
--     add_files("../test-main.c")
--     add_defines("LIBCA_USE_TEST")
--     add_deps("ca-c-container")
--     add_links("ca-c-container")
--     add_deps("ca-c-core")
--     add_links("ca-c-core")

-- 嵌入式总库
target("ca-em")
    set_kind("static")
    add_deps("ca-base")
    add_deps("ca-em_util")
    add_deps("ca-em_driver")
    add_deps("ca-em_log")
    add_deps("ca-em_collection")

-------------------------------------------------------------------------------
-- C++的库
