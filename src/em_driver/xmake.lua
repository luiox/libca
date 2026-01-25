-- 嵌入式驱动库
target("ca-em_driver")
    set_kind("static")
    add_files("**.c")
    add_deps("ca-em_base")

local function get_supported_modules()
    -- Only scan the project's src/em_driver directory (top-level .c files)
    local basedir = path.join(os.projectdir(), "src", "em_driver")
    local files = os.files(path.join(basedir, "*.c"))
    local modules = {}
    if not files or #files == 0 then
        return modules
    end
    for _, f in ipairs(files) do
        local mod = f:match("([^/\\]+)%.c$")
        if mod then
            table.insert(modules, mod)
        end
    end
    table.sort(modules)
    return modules
end

local function print_supported_modules()
    local modules = get_supported_modules()
    if #modules == 0 then
        print("[libca.em.driver] no .c files found in " .. os.scriptdir())
        return
    end
    print("[libca.em.driver] supported modules:")
    for _, mod in ipairs(modules) do
        print("  - " .. mod)
    end
end

-- task to let user list modules via `xmake show-em-drivers`
task("show-em-drivers")
    set_category("utils")
    set_menu({
        usage = "xmake show-em-drivers",
        description = "Print supported em driver modules",
    })
    on_run(function ()
        print_supported_modules()
    end)

rule("libca.em.driver")
    on_load(function (target)
        -- print supported modules when rule is loaded
        print_supported_modules()
    end)

