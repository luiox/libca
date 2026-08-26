-- libca.test：基于标记文件的多项目测试布局与样本定位。
--
-- 消费方式（测试 target）：
--   add_deps("libca_test")
--   set_rundir("$(projectdir)")   -- 使 CWD = 顶层仓根
--
--   int main() {
--       ca::test::setup("my_project");
--       auto data = ca::test::resource("samples/a.bin");
--   }

target("libca_test")
    set_kind("static")
    set_group("libs")
    add_deps("libca_core")
    add_files("src/libca/test/*.cpp")
    add_headerfiles("src/(libca/test/*.hpp)")
    add_includedirs("src", {public = true})
    if is_plat("windows") then
        add_cxflags("/utf-8", {tools = "cl"})
    end

if has_config("with_tests") then
target("libca_test_unittest")
    set_kind("binary")
    set_default(false)
    add_tests("default")
    set_group("libs/test")
    add_deps("libca_test")
    add_links("libca_test", "libca_core")
    add_packages("gtest")
    add_files("unittest/main.cpp")
    add_files("unittest/*_test.cpp")
    -- CWD = 本模块目录：扫描从这里开始，fixture 树见 unittest/。
    set_rundir("$(projectdir)/libca/test")
    if is_plat("windows") then
        add_cxflags("/utf-8", {tools = "cl"})
    end
end
