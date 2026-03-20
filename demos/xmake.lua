set_project("libca-demos")
set_version("0.0.1")
set_xmakever("2.8.3")
set_languages("c99")

add_rules("mode.debug", "mode.release")

add_moduledirs(path.join(os.scriptdir(), "..", "xmake", "modules"))

target("demo_em_modules")
    set_kind("binary")
    add_files("app/main_modules.c", "app/log_deps_stub.c")
    on_load(function (target)
        local em = import("libca.em")
        em.setup(target, {
            root = path.join(os.scriptdir(), "..")
        })

        em.add_libs(target, "em_log", {
            backend = "simple_logger"
        })
    end)
