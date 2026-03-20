set_project("libca-demo")
set_version("0.0.1")
set_xmakever("2.8.3")
set_languages("c99")

add_rules("mode.debug", "mode.release")

-- 模拟用户工程：只通过 import 模块接入 libca 源码包
add_moduledirs(path.join(os.projectdir(), "xmake", "modules"))

target("demo_led_extern")
    set_kind("binary")
    add_files("app/main.c")
    on_load(function (target)
        local em = import("libca.em")
        em.setup(target, {
            root = os.projectdir()
        })

        em.add_libs(target, "em_base", {
            enable = true
        })

        em.add_libs(target, "em_driver", {
            led = {
                mode = "extern",
                port = {path.join(os.scriptdir(), "board", "port_led.c")}
            }
        })
    end)
