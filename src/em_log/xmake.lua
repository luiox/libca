-- 嵌入式日志库
target("libca.em_log")
    set_kind("static")
    set_group("em")
    add_files("log.c")
    add_deps("libca.em_base")  

target("test-log")
    set_kind("binary")
    add_rules("em_test", { test_enable = true, use_default_main = true })
    add_files("log.c")
    add_defines("USE_CUSTOM_CPU_ADAPTER=1")
    if is_os("windows") then
        add_files("../em_platform/adapters/win32/cpu_adapter.c")
    end 
    if is_os("linux") then 
        add_files("../em_platform/adapters/linux/cpu_adapter.c")
    end
  
    add_deps("libca.em_base")
    add_deps("libca.em_util")
    add_deps("libca.em_platform")