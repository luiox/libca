target("libca.em_crypto")
    set_kind("static")
    add_files("*.c")
    add_deps("libca.em_base")

local em_crypto_test_dir = "$(projectdir)/tests/em_crypto"

target("test-em_crypto")
    set_kind("binary")
    add_includedirs(".", em_crypto_test_dir)
    add_files("crypto.c", "base64.c")
    add_files(path.join(em_crypto_test_dir, "test_crypto.c"))
    add_files(path.join(em_crypto_test_dir, "test_base64.c"))
    add_rules("em_test", { test_enable = true, use_default_main = true })
    add_deps("libca.em_base")
