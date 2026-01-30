-- 嵌入式驱动库
local log = new_logger("em_driver")

local function define_em_driver(name, default_config)
    target(name)
        set_kind("object")
        add_deps("libca.em_base")
        -- 根据配置加源码
        on_load(function (target)
            -- 1. 获取配置参数
            local config = target:get("config") or default_config or {}
            -- print(">>> [DEBUG] config type:", type(config))
            -- print(">>> [DEBUG] config content:", config) 
            local is_filter_enabled = config.enable_filter ~= false
            local wanted_modules = config.drivers or {}
            
            local files_to_compile = {}
            
            -- 获取全局配置，看是否要显示驱动加载的详细信息
            -- local show_info = has_config("show_driver_info") -- 已经封装到log里了

            -- 2. 决定要扫描哪些文件
            if not is_filter_enabled then
                -- 【调试模式】：扫描目录下所有 .c 文件
                log.debug("Filter disabled. Scanning all files.")
                
                local all_files = os.files(path.join(os.scriptdir(), "**.c"))
                for _, f in ipairs(all_files) do
                    table.insert(files_to_compile, f)
                end
                
            else
                -- 【生产模式】：只扫描用户指定的模块
                -- 获取当前脚本的目录，确保路径准确
                local base_dir = os.scriptdir() 
                
                for _, mod_name in ipairs(wanted_modules) do
                    -- 拼凑文件名，假设文件名为 mod_name.c
                    local filepath = path.join(base_dir, mod_name .. ".c")
                    
                    -- 检查文件是否存在
                    if os.isfile(filepath) then
                        table.insert(files_to_compile, filepath)
                    else
                        -- 可选：如果用户指定了一个不存在的驱动，给个提示
                        log.warn("module '" .. mod_name .. "' not found, skipping.")
                    end
                end
            end
            
            -- 3. 批量添加筛选后的文件
            if #files_to_compile > 0 then
                target:add("files", files_to_compile)
                
                -- 打印结果
                local enabled_list = {}
                for _, f in ipairs(files_to_compile) do
                    table.insert(enabled_list, path.basename(path.filename(f)))
                end
                table.sort(enabled_list)
                
                log.info("enabled modules (" .. #enabled_list .. "):")
                if #enabled_list > 0 then
                    print("- " .. table.concat(enabled_list, ", "))
                end
            else
                log.warn("No modules enabled!")
            end
        end)
end

define_em_driver("libca.em_driver", {enable_filter = true})
define_em_driver("libca.em_driver.all", {enable_filter = false})

target("libca.test-em_driver")
    set_kind("static")
    add_deps("libca.em_driver.all")

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

