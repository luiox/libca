set_project("libca")
set_version("0.0.1")
set_xmakever("2.8.3")

set_languages("c99", "cxx17")

add_rules("mode.debug", "mode.release")
add_rules("plugin.compile_commands.autoupdate", {outputdir = "."})

add_requires("doctest 2.4.11")
add_requires("trompeloeil 47")
add_requires("nanobench 4.3.11")
add_requires("spdlog 1.14.1")

add_subdirs("src/c")
add_subdirs("src/cpp")

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

