target("libca.em_mpool")
	set_kind("static")
	set_group("em")
	add_files("fixed_allocator.c")
	add_deps("libca.em_base", "libca.em_util")
	add_includedirs("..", {public = true})

local em_mpool_test_dir = "$(projectdir)/tests/em_mpool"

target("test-fixed_allocator")
	set_kind("binary")
	set_group("test")
	add_includedirs(".", em_mpool_test_dir)
	add_files("fixed_allocator.c", path.join(em_mpool_test_dir, "test_fixed_allocator.c"))
	add_rules("em_test", { test_enable = true, use_default_main = true })
	add_deps("libca.em_base", "libca.em_util")
