set_project("libca-install-consumer")
set_version("0.0.1")
set_xmakever("2.8.3")
set_languages("cxx17")

option("libca_install_dir")
    set_showmenu(true)
    set_description("Path produced by xmake install")
option_end()

target("libca_install_consumer")
    set_kind("binary")
    add_files("main.cpp")
    if is_plat("windows") then
        add_cxflags("/utf-8", {tools = "cl"})
    end

    on_load(function (target)
        local install_dir = get_config("libca_install_dir")
        if not install_dir or install_dir == "" then
            raise("libca_install_dir is required")
        end

        target:add("includedirs", path.join(install_dir, "include"))
        target:add("linkdirs", path.join(install_dir, "lib"))
        target:add("links", "libca_json", "libca_str", "libca_core")
    end)
