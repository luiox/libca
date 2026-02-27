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

-- format的单元测试
target("test-format")
    set_kind("binary")
    add_files("format.c")
    add_rules("em_test", { test_enable = true, use_default_main = true })

target("test-format-float-fixed")
    set_kind("binary")
    add_files("format.c")
    add_defines("FMT_ENABLE_FLOAT=1")
    add_defines("FMT_ENABLE_WIDTH_PRECISION=1")
    add_defines("FMT_ENABLE_HEX=1")
    add_defines("FMT_FLOAT_MODE=FMT_FLOAT_MODE_FIXED")
    add_rules("em_test", { test_enable = true, use_default_main = true })

target("test-format-float-simple")
    set_kind("binary")
    add_files("format.c")
    add_defines("FMT_ENABLE_FLOAT=1")
    add_defines("FMT_ENABLE_WIDTH_PRECISION=1")
    add_defines("FMT_ENABLE_HEX=1")
    add_defines("FMT_FLOAT_MODE=FMT_FLOAT_MODE_SIMPLE")
    add_rules("em_test", { test_enable = true, use_default_main = true })

target("test-format-float-normal")
    set_kind("binary")
    add_files("format.c")
    add_defines("FMT_ENABLE_FLOAT=1")
    add_defines("FMT_ENABLE_WIDTH_PRECISION=1")
    add_defines("FMT_ENABLE_HEX=1")
    add_defines("FMT_FLOAT_MODE=FMT_FLOAT_MODE_NORMAL")
    add_rules("em_test", { test_enable = true, use_default_main = true })

