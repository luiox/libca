-- C的测试库，主要是用于C的单元测试
-- 定义测试框架规则 (rule)
-- rule 适用于定义可复用的构建逻辑，比如“如何处理测试组件”
rule("em_test")
    on_load(function (target)
        -- extraconf 用于获取 add_rules("em_test", {configs}) 中传入的额外配置信息
        -- 第一个参数是类型 "rules"，第二个是规则名 "em_test"
        local configs = target:extraconf("rules", "em_test")
        
        -- 自动为使用此规则的目标添加头文件搜索路径
        target:add("includedirs", os.scriptdir(), {public = true})
        
        -- 设置组别为 test，方便管理
        target:set("group", "em/test")
        
        -- 自动将目标注册为 xmake test 可识别的测试项
        target:add("tests", target:name())
        if configs then
            -- 如果配置了 test_enable，则开启测试宏
            if configs.test_enable then
                target:add("defines", "TEST_ENABLE=1")
            end
            -- 如果配置了 use_default_main，则注入测试框架自带的 main 函数源码
            if configs.use_default_main then
                target:add("defines", "TEST_SELF_MAIN=1")
                -- 使用 path.join 和 os.scriptdir() 确保路径在不同目录下调用时依然正确
                target:add("files", path.join(os.scriptdir(), "test_main.c"))
            end
        end
    end)

-- 最小的自测试可执行文件，仅运行测试框架，测试自身是否有问题
target("test-em_self_test")
    set_kind("binary")

    -- 应用 em_test 规则，并通过第二个参数（table）传递自定义配置
    add_rules("em_test", { test_enable = true, use_default_main = true })
    
    -- 只需添加业务测试代码，框架代码由 rule 自动注入
    add_files("test.c")

