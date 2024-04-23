-- task("find_tests")
--     set_menu {
--         usage = "xmake find_tests [options]",
--         description = "Find and run all tests in subdirectories!",
--         options = {}
--     }
--     on_run(function ()
--         local function find_xmake_files_in_dir(dir)
--             local xmake_files = {}
--             local entries = os.dirs(dir)
--             for _, entry in ipairs(entries) do
--                 local path = path.insert(dir, entry)
--                 local xmake_file = path.insert(path, "xmake.lua")
--                 if os.exists(xmake_file) then
--                     table.insert(xmake_files, xmake_file)
--                 end
--             end
--             return xmake_files
--         end

--         local function find_and_run_tests_in_dir(dir)
--             local xmake_files = find_xmake_files_in_dir(dir)
--             for _, xmake_file in ipairs(xmake_files) do
--                 os.cd(path.directory(xmake_file))
--                 local ok, targets = project.load()
--                 if ok then
--                     for _, target in ipairs(targets) do
--                         local targetname = target:name()
--                         if targetname:find("test-*") then
--                             print("Found test target: " .. targetname)
--                             os.exec("xmake run %s", targetname)
--                         end
--                     end
--                 end
--             end
--         end

--         find_and_run_tests_in_dir(".")
--     end)

includes("./*/xmake.lua")

task("tests")
    set_menu {usage = "xmake tests [options]" , description = "Run all tests!", options = {}}
    on_run(function ()
        import("core.project.project")
        for _, target in pairs(project.targets()) do
            local targetname = target:name()
            if targetname:find("test-*") then
                print("Found test target: " .. targetname)
                -- os.exec("xmake build %s", targetname)
                -- os.exec("xmake run %s", targetname)
                local cmd = "xmake build " .. targetname
                os.execv(cmd, {curdir = os.projectdir(), stdout = io.stdout, stderr = io.stderr})
                cmd = "xmake run " .. targetname
                os.execv(cmd, {curdir = os.projectdir(), stdout = io.stdout, stderr = io.stderr})
            end
        end
    end)
