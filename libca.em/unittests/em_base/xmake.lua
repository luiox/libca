local src_dir = "$(projectdir)/libca.em/src/em_base"

-- User-mode tests import libca.em from source package manager entry.
add_moduledirs(path.join(os.projectdir(), "xmake", "modules"))

target("test-datatype")
    set_kind("binary")
    add_includedirs(src_dir, ".")
    add_files("test_datatype.c")
    add_rules("em_test", { test_enable = true, use_default_main = true })

target("test-debug")
    set_kind("binary")
    add_includedirs(src_dir, ".")
    add_files(path.join(src_dir, "debug.c"), "test_debug.c")
    add_rules("em_test", { test_enable = true, use_default_main = true })

target("test-compiler_compat")
    set_kind("binary")
    add_includedirs(src_dir, ".")
    add_files("test_compiler_compat.c")
    add_rules("em_test", { test_enable = true, use_default_main = true })

target("test-macro_util")
    set_kind("binary")
    add_includedirs(src_dir, ".")
    add_files(path.join(src_dir, "macro_util.c"), "test_macro_util.c")
    add_rules("em_test", { test_enable = true, use_default_main = true })

target("test-string_util_std")
    set_kind("binary")
    add_includedirs(src_dir, ".")
    add_files(path.join(src_dir, "string_util.c"), "test_string_util.c")
    add_rules("em_test", { test_enable = true, use_default_main = true })

target("test-string_util_custom")
    set_kind("binary")
    add_includedirs(src_dir, ".")
    add_files(path.join(src_dir, "string_util.c"), "test_string_util.c")
    add_rules("em_test", { test_enable = true, use_default_main = true })
    add_defines("USE_CUSTOM_STRING_UTIL_IMPL=1")

target("test-memory_util_std")
    set_kind("binary")
    add_includedirs(src_dir, ".")
    add_files(path.join(src_dir, "memory_util.c"), "test_memory_util.c")
    add_rules("em_test", { test_enable = true, use_default_main = true })

target("test-memory_util_custom")
    set_kind("binary")
    add_includedirs(src_dir, ".")
    add_files(path.join(src_dir, "memory_util.c"), "test_memory_util.c")
    add_rules("em_test", { test_enable = true, use_default_main = true })
    add_defines("USE_CUSTOM_MEMORY_UTIL_IMPL=1")

-- Cross-check matrix: memory_util/string_util independent configuration.
target("test-base_impl_std_std")
    set_kind("binary")
    add_includedirs(src_dir, ".")
    add_files(
        path.join(src_dir, "memory_util.c"),
        path.join(src_dir, "string_util.c"),
        "test_memory_util.c",
        "test_string_util.c"
    )
    add_rules("em_test", { test_enable = true, use_default_main = true })

target("test-base_impl_std_custom")
    set_kind("binary")
    add_includedirs(src_dir, ".")
    add_files(
        path.join(src_dir, "memory_util.c"),
        path.join(src_dir, "string_util.c"),
        "test_memory_util.c",
        "test_string_util.c"
    )
    add_rules("em_test", { test_enable = true, use_default_main = true })
    add_defines("USE_CUSTOM_STRING_UTIL_IMPL=1")

target("test-base_impl_custom_std")
    set_kind("binary")
    add_includedirs(src_dir, ".")
    add_files(
        path.join(src_dir, "memory_util.c"),
        path.join(src_dir, "string_util.c"),
        "test_memory_util.c",
        "test_string_util.c"
    )
    add_rules("em_test", { test_enable = true, use_default_main = true })
    add_defines("USE_CUSTOM_MEMORY_UTIL_IMPL=1")

target("test-base_impl_custom_custom")
    set_kind("binary")
    add_includedirs(src_dir, ".")
    add_files(
        path.join(src_dir, "memory_util.c"),
        path.join(src_dir, "string_util.c"),
        "test_memory_util.c",
        "test_string_util.c"
    )
    add_rules("em_test", { test_enable = true, use_default_main = true })
    add_defines("USE_CUSTOM_MEMORY_UTIL_IMPL=1", "USE_CUSTOM_STRING_UTIL_IMPL=1")

-- Source-package manager mode tests: simulate external user integration.
target("test-base_user_mode_std_std")
    set_kind("binary")
    add_includedirs(src_dir, ".")
    add_files("test_memory_util.c", "test_string_util.c")
    add_rules("em_test", { test_enable = true, use_default_main = true })
    on_load(function (target)
        local em = import("libca.em")
        em.setup(target, {
            root = os.projectdir()
        })
        em.add_libs(target, "em_base", {
            memory_util = "std",
            string_util = "std"
        })
    end)

target("test-base_user_mode_custom_custom")
    set_kind("binary")
    add_includedirs(src_dir, ".")
    add_files("test_memory_util.c", "test_string_util.c")
    add_rules("em_test", { test_enable = true, use_default_main = true })
    on_load(function (target)
        local em = import("libca.em")
        em.setup(target, {
            root = os.projectdir()
        })
        em.add_libs(target, "em_base", {
            memory_util = "custom",
            string_util = "custom"
        })
    end)