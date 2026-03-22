local src_dir = "$(projectdir)/src/em_mpool"

target("test-fixed_allocator")
    set_kind("binary")
    set_group("test")
    add_includedirs(src_dir, ".")
    add_files(path.join(src_dir, "fixed_allocator.c"), "test_fixed_allocator.c")
    add_rules("em_test", { test_enable = true, use_default_main = true })
    add_deps("libca.em_base", "libca.em_util")
