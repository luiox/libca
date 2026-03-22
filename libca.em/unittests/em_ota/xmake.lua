local src_dir = "$(projectdir)/src/em_ota"

target("test-em_ota")
    set_kind("binary")
    add_includedirs(src_dir, ".")
    add_files(path.join(src_dir, "partition.c"), "test_partition.c")
    add_rules("em_test", { test_enable = true, use_default_main = true })
    add_deps("libca.em_base")
