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
        -- 闭包须与包定义 MODULE_DEPS 一致：json 解析文件流经 fs::Path 打开（0.0.14 起
        -- json 依赖 fs），漏链 libca_fs 会在 LNK2019 报 from_utf8_lossy/native 未解析。
        target:add("links", "libca_json", "libca_fs", "libca_str", "libca_core")
    end)
