set_project("libca-demo")
set_version("0.0.1")
set_xmakever("2.8.3")
set_languages("c99")

add_rules("mode.debug", "mode.release")

-- 模拟用户工程：只通过 import 模块接入 libca 源码包
add_moduledirs(path.join(os.scriptdir(), "..", "xmake", "modules"))

target("demo_led_extern")
    set_kind("binary")
    add_files("app/main.c")
    on_load(function (target)
        local em = import("libca.em")
        em.setup(target, {
            root = path.join(os.scriptdir(), "..")
        })

        em.add_libs(target, "em_driver", {
            led = {
                mode = "extern",
                port = {path.join(os.scriptdir(), "board", "port_led.c")}
            }
        })
    end)

target("demo_led_dynamic")
    set_kind("binary")
    add_files("app/main_dynamic.c")
    on_load(function (target)
        local em = import("libca.em")
        em.setup(target, {
            root = path.join(os.scriptdir(), "..")
        })

        em.add_libs(target, "em_driver", {
            led = {
                mode = "dynamic"
            }
        })
    end)

target("demo_led_default_port")
    set_kind("binary")
    add_files("app/main.c")
    on_load(function (target)
        local em = import("libca.em")
        em.setup(target, {
            root = path.join(os.scriptdir(), "..")
        })

        em.add_libs(target, "em_driver", {
            led = {
                mode = "extern"
            }
        })
    end)

target("demo_driver_manifests_check")
    set_kind("binary")
    add_files("app/main.c")
    on_load(function (target)
        local root = path.join(os.scriptdir(), "..")
        local driver_root = path.join(root, "src", "em_driver")

        for _, dir in ipairs(os.dirs(path.join(driver_root, "*"))) do
            local driver_name = path.basename(dir)
            local manifest = path.join(dir, driver_name .. ".lua")
            if not os.isfile(manifest) then
                raise("demo check: missing driver manifest %s", manifest)
            end
        end

        local em = import("libca.em")
        em.setup(target, {
            root = root
        })
        em.add_libs(target, "em_driver", {
            led = {
                mode = "extern"
            }
        })
    end)
