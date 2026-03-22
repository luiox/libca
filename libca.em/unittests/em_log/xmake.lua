local src_dir = "$(projectdir)/libca.em/src/em_log"

target("test-log")
    set_kind("binary")
    add_includedirs(src_dir, ".")
    add_files(path.join(src_dir, "log.c"), "test_log.c")
    add_files("$(projectdir)/libca.em/src/em_dstream/ring_buffer.c")
    add_files("$(projectdir)/libca.em/src/em_platform/async.c")
    add_files("$(projectdir)/libca.em/src/em_platform/time_util.c")
    add_defines("USE_CUSTOM_CPU_ADAPTER=1")
    if is_os("windows") then
        add_files("$(projectdir)/libca.em/src/em_platform/adapters/win32/cpu_adapter.c")
    end
    if is_os("linux") then
        add_files("$(projectdir)/libca.em/src/em_platform/adapters/linux/cpu_adapter.c")
    end
    add_rules("em_test", { test_enable = true, use_default_main = true })
    add_deps("libca.em_base", "libca.em_util")

target("test-simple_logger")
    set_kind("binary")
    add_includedirs(src_dir, ".")
    add_files(path.join(src_dir, "simple_logger.c"), "test_simple_logger.c")
    add_rules("em_test", { test_enable = true, use_default_main = true })
    add_deps("libca.em_base")

target("test-simple_logger-fast")
    set_kind("binary")
    add_includedirs(src_dir, ".")
    add_files(path.join(src_dir, "simple_logger.c"), "test_simple_logger.c")
    add_files("$(projectdir)/libca.em/src/em_format/format.c")
    add_defines("SLOG_USE_FAST_VSNPRINTF=1")
    add_defines("FMT_ENABLE_FLOAT=1")
    add_defines("FMT_ENABLE_WIDTH_PRECISION=1")
    add_defines("FMT_ENABLE_HEX=1")
    add_defines("FMT_FLOAT_MODE=FMT_FLOAT_MODE_SIMPLE")
    add_rules("em_test", { test_enable = true, use_default_main = true })
    add_deps("libca.em_base")
