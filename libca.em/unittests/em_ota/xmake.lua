local src_dir = "$(projectdir)/libca.em/src/em_ota"

target("test-em_ota")
    set_kind("binary")
    add_includedirs(src_dir, ".")
    add_files(path.join(src_dir, "partition.c"), "test_partition.c")
    add_rules("em_test", { test_enable = true, use_default_main = true })
    add_deps("libca.em_base")

target("test-ota_image")
    set_kind("binary")
    set_group("em/test")
    add_includedirs(src_dir, ".")
    add_files(path.join(src_dir, "partition.c"), path.join(src_dir, "ota_image.c"), "test_ota_image.c")
    add_rules("em_test", { test_enable = true, use_default_main = true })
    add_deps("libca.em_base", "libca.em_util")
