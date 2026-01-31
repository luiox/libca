target("libca.em_base")
    set_kind("object")
    set_group("em")
    add_files("**.c")

target("libca.em_base_static")
    set_kind("static")
    set_group("em")
    add_files("**.c")

-- datatype的单元测试
target("test-datatype")
    set_kind("binary")
    add_files("datatype.c")
    add_rules("em_test", { test_enable = true, use_default_main = true })

-- debug的单元测试
target("test-debug")
    set_kind("binary")
    add_files("debug.c")
    add_rules("em_test", { test_enable = true, use_default_main = true })

-- compiler_compat的单元测试
target("test-compiler_compat")
    set_kind("binary")
    add_files("compiler_compat.c")
    add_rules("em_test", { test_enable = true, use_default_main = true })

-- macro_util的单元测试
target("test-macro_util")
    set_kind("binary")
    add_files("macro_util.c")
    add_rules("em_test", { test_enable = true, use_default_main = true })

-- string_util的单元测试
target("test-string_util")
    set_kind("binary")
    add_files("string_util.c")
    add_rules("em_test", { test_enable = true, use_default_main = true })

-- memory_util的单元测试
target("test-memory_util")
    set_kind("binary")
    add_files("memory_util.c")
    add_rules("em_test", { test_enable = true, use_default_main = true })

