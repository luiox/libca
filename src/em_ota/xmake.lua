target("libca.em_ota")
    set_kind("static")
    add_files("partition.c")
    add_deps("libca.em_base")
    add_includedirs("..", {public = true})

local em_ota_test_dir = "$(projectdir)/tests/em_ota"

target("test-em_ota")
    set_kind("binary")
    add_includedirs(".", em_ota_test_dir)
    add_files("partition.c", path.join(em_ota_test_dir, "test_partition.c"))
    add_rules("em_test", { test_enable = true, use_default_main = true })
    add_deps("libca.em_base")
