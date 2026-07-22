set_project("libca")
set_version("0.0.1")
set_xmakever("2.8.3")

option("with_core")
    set_default(true)
    set_showmenu(true)
    set_description("Enable libca (C++ core) targets")
option_end()

option("with_em")
    set_default(true)
    set_showmenu(true)
    set_description("Enable libca.em targets")
option_end()

option("with_demo")
    set_default(true)
    set_showmenu(true)
    set_description("Enable demo targets")
option_end()

-- 测试开关：控制是否拉取 gtest 并启用 *_unittest target。
-- 默认 false：libca 独立构建跑测试需 `xmake f --with_tests=y`。
-- 作为 submodule 被 includes 时（如 morpher 直接 includes 子库 xmake.lua），
-- 默认不构建测试，避免强制拉取 gtest / 强制定义 *_unittest target。
option("with_tests")
    set_default(false)
    set_showmenu(true)
    set_description("Enable *_unittest targets and pull gtest via xrepo.")
option_end()

option("with_openssl")
    set_default(false)
    set_showmenu(true)
    set_description("Enable optional OpenSSL HTTPS client support")
option_end()

if has_config("with_tests") then
    add_requires("gtest")
end

if has_config("with_openssl") then
    add_requires("openssl3")
end

if is_plat("windows") then
    -- Xmake maps c99 to /TP for MSVC; C11 keeps em sources in C mode.
    set_languages("c11", "cxx17")
else
    set_languages("c99", "cxx17")
end

add_rules("mode.debug", "mode.release", "mode.coverage")
add_rules("plugin.compile_commands.autoupdate", {outputdir = "."})
-- 如果上面这个不行使用下面这个命令
-- xmake project -k compile_commands

-- 设置 C 编译选项
-- add_cflags("-finput-charset=UTF-8", "-fexec-charset=UTF-8")

-- 设置 C++ 编译选项
-- add_cxflags("-finput-charset=UTF-8", "-fexec-charset=UTF-8")

-- 如果是 msvc 编译器，则使用以下选项
if is_plat("windows") then 
    add_cflags("/utf-8")
    add_cxflags("/utf-8")
end

-- add_requires("doctest 2.4.11")
-- add_requires("trompeloeil 47")
-- add_requires("nanobench 4.3.11")
-- add_requires("spdlog 1.14.1")

on_load(function (target)
    -- 检查当前是否是 coverage 模式
    if is_mode("coverage") then
        -- 仅为 gcc/clang 添加 gcov 标志；跳过 MSVC（Windows 的覆盖率工具不同）
        if is_plat("windows") then
            return
        end

        -- 添加编译/链接标志以生成 gcno/gcda
        target:add("cflags", "-fprofile-arcs", "-ftest-coverage", "-fno-omit-frame-pointer")
        target:add("cxxflags", "-fprofile-arcs", "-ftest-coverage", "-fno-omit-frame-pointer")
        target:add("ldflags", "-fprofile-arcs", "-ftest-coverage", {force = true})

        -- 减少内联以提高覆盖率准确性，并保留调试符号
        target:add("cxflags", "-fno-inline")
        target:set("symbols", "debug")
        target:add("defines", "COVERAGE_BUILD")
    end
end)

includes("xmake/modules/libca/tool/logger.lua")

if has_config("with_em") then
    includes("libca.em")
end

if has_config("with_core") then
    includes("libca")
end

-- -- task("find_tests")
-- --     set_menu {
-- --         usage = "xmake find_tests [options]",
-- --         description = "Find and run all tests in subdirectories!",
-- --         options = {}
-- --     }
-- --     on_run(function ()
-- --         local function find_xmake_files_in_dir(dir)
-- --             local xmake_files = {}
-- --             local entries = os.dirs(dir)
-- --             for _, entry in ipairs(entries) do
-- --                 local path = path.insert(dir, entry)
-- --                 local xmake_file = path.insert(path, "xmake.lua")
-- --                 if os.exists(xmake_file) then
-- --                     table.insert(xmake_files, xmake_file)
-- --                 end
-- --             end
-- --             return xmake_files
-- --         end

-- --         local function find_and_run_tests_in_dir(dir)
-- --             local xmake_files = find_xmake_files_in_dir(dir)
-- --             for _, xmake_file in ipairs(xmake_files) do
-- --                 os.cd(path.directory(xmake_file))
-- --                 local ok, targets = project.load()
-- --                 if ok then
-- --                     for _, target in ipairs(targets) do
-- --                         local targetname = target:name()
-- --                         if targetname:find("test-*") then
-- --                             print("Found test target: " .. targetname)
-- --                             os.exec("xmake run %s", targetname)
-- --                         end
-- --                     end
-- --                 end
-- --             end
-- --         end

-- --         find_and_run_tests_in_dir(".")
-- --     end)

-- includes("./*/xmake.lua")

-- task("tests")
--     set_menu {usage = "xmake tests [options]" , description = "Run all tests!", options = {}}
--     on_run(function ()
--         import("core.project.project")
--         for _, target in pairs(project.targets()) do
--             local targetname = target:name()
--             if targetname:find("test-*") then
--                 print("Found test target: " .. targetname)
--                 -- os.exec("xmake build %s", targetname)
--                 -- os.exec("xmake run %s", targetname)
--                 local cmd = "xmake build " .. targetname
--                 os.execv(cmd, {curdir = os.projectdir(), stdout = io.stdout, stderr = io.stderr})
--                 cmd = "xmake run " .. targetname
--                 os.execv(cmd, {curdir = os.projectdir(), stdout = io.stdout, stderr = io.stderr})
--             end
--         end
--     end)


--
-- If you want to known more usage about xmake, please see https://xmake.io
--
-- ## FAQ
--
-- You can enter the project directory firstly before building project.
--
--   $ cd projectdir
--
-- 1. How to build project?
--
--   $ xmake
--
-- 2. How to configure project?
--
--   $ xmake f -p [macosx|linux|iphoneos ..] -a [x86_64|i386|arm64 ..] -m [debug|release]
--
-- 3. Where is the build output directory?
--
--   The default output directory is `./build` and you can configure the output directory.
--
--   $ xmake f -o outputdir
--   $ xmake
--
-- 4. How to run and debug target after building project?
--
--   $ xmake run [targetname]
--   $ xmake run -d [targetname]
--
-- 5. How to install target to the system directory or other output directory?
--
--   $ xmake install
--   $ xmake install -o installdir
--
-- 6. Add some frequently-used compilation flags in xmake.lua
--
-- @code
--    -- add debug and release modes
--    add_rules("mode.debug", "mode.release")
--
--    -- add macro definition
--    add_defines("NDEBUG", "_GNU_SOURCE=1")
--
--    -- set warning all as error
--    set_warnings("all", "error")
--
--    -- set language: c99, c++11
--    set_languages("c99", "c++11")
--
--    -- set optimization: none, faster, fastest, smallest
--    set_optimize("fastest")
--
--    -- add include search directories
--    add_includedirs("/usr/include", "/usr/local/include")
--
--    -- add link libraries and search directories
--    add_links("tbox")
--    add_linkdirs("/usr/local/lib", "/usr/lib")
--
--    -- add system link libraries
--    add_syslinks("z", "pthread")
--
--    -- add compilation and link flags
--    add_cxflags("-stdnolib", "-fno-strict-aliasing")
--    add_ldflags("-L/usr/local/lib", "-lpthread", {force = true})
--
-- @endcode
--

