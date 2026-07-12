-- gtest 由根 xmake.lua 的 with_tests option 统一管理（默认 false）。

target("libca_fs")
    set_kind("static")
    set_group("libs")
    add_files("src/libca/fs/*.cpp")
    add_headerfiles("src/libca/fs/*.hpp")
    add_includedirs("src", {public = true})
    add_deps("libca_core")

    if is_plat("windows") then
        add_cxflags("/utf-8", {tools = "cl"})
    end

    -- MinGW/GCC 需要链接 stdc++fs 库（GCC < 9）或内置支持（GCC >= 9）
    on_load(function(target)
        if is_plat("mingw") or is_plat("linux") then
            local gcc_ver = try { function() return target:toolchain("gcc"):version() end }
            if gcc_ver and gcc_ver < "9.0" then
                target:add("ldflags", "-lstdc++fs")
            end
        end
    end)

if has_config("with_tests") then
target("libca_fs_unittest")
    set_kind("binary")
    set_group("libs/test")
    add_deps("libca_fs")
    add_links("libca_fs", "libca_core")
    add_packages("gtest")
    add_files("unittest/main.cpp")
    add_files("unittest/*_test.cpp")
    add_includedirs("src")
    set_rundir("$(projectdir)")
    if is_plat("windows") then
        add_cxflags("/utf-8", {tools = "cl"})
    end
end
