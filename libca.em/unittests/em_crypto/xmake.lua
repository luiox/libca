local src_dir = "$(projectdir)/src/em_crypto"

target("test-em_crypto")
    set_kind("binary")
    add_includedirs(src_dir, ".")
    add_files(path.join(src_dir, "crypto.c"), path.join(src_dir, "base64.c"))
    add_files("test_crypto.c", "test_base64.c")
    add_rules("em_test", { test_enable = true, use_default_main = true })
    add_deps("libca.em_base")
