-- 嵌入式驱动库

-- 获取 rule 配置，并校验 mode/port 的通用合法性。
local function em_driver_get_rule_options(target, rule_name)
	local opts = target:extraconf("rules", rule_name) or {}
	local mode = opts.mode or "extern"

	if mode ~= "extern" and mode ~= "dynamic" then
		raise("%s: invalid mode '%s', expected 'extern' or 'dynamic'", rule_name, tostring(mode))
	end

	if opts.port ~= nil and type(opts.port) ~= "table" then
		raise("%s: 'port' must be a list(table), for example {\"1.c\", \"2.c\"}", rule_name)
	end

	return opts, mode
end

-- 将用户传入的 port 列表归一化为绝对路径，便于跨目录 rule 复用。
local function em_driver_normalize_port_files(rule_name, port_files)
	if type(port_files) ~= "table" or #port_files == 0 then
		return {}
	end

	local normalized_files = {}
	for _, f in ipairs(port_files) do
		if type(f) ~= "string" then
			raise("%s: each item in 'port' must be a file path string", rule_name)
		end
		local fp = path.is_absolute(f) and f or path.absolute(f, os.projectdir())
		table.insert(normalized_files, fp)
	end
	return normalized_files
end

-- driver rule 通用注入逻辑：核心源码 + mode 宏 + extern 端口文件策略。
local function em_driver_apply_rule(target, cfg)
	local rule_name = cfg.rule_name
	local driver_dir = cfg.driver_dir
	local core_files = cfg.core_files or {}

	local opts, mode = em_driver_get_rule_options(target, rule_name)
	local port_files = em_driver_normalize_port_files(rule_name, opts.port)

	local src_root = path.join(driver_dir, "..", "..")
	target:add("includedirs", src_root)

	for _, f in ipairs(core_files) do
		target:add("files", path.join(driver_dir, f))
	end

	if mode == "dynamic" then
		target:add("defines", cfg.dynamic_define)
		return
	end

	target:add("defines", cfg.extern_define)

	if #port_files > 0 then
		target:add("files", port_files)
	elseif cfg.default_port_file then
		-- extern mode without user port list: use built-in weak default port
		target:add("files", path.join(driver_dir, cfg.default_port_file))
	end
end

-- LED 驱动规则示例
rule("libca.em_driver.led")
	on_load(function (target)
		em_driver_apply_rule(target, {
			rule_name = "libca.em_driver.led",
			driver_dir = path.join(os.scriptdir(), "led"),
			core_files = {"led.c"},
			default_port_file = "port_led.c",
			extern_define = "LIBCA_LED_PORT_MODE=1",
			dynamic_define = "LIBCA_LED_PORT_MODE=2"
		})
	end)
rule_end()

-- 添加子目录内的驱动 rule。
includes("led/xmake.lua")

---------------------------------------------------
-- 老式的扫描法先移除

-- local log = new_logger("em_driver")

-- local function dedup_and_sort(items)
--     local exists = {}
--     local result = {}
--     for _, item in ipairs(items or {}) do
--         if item and not exists[item] then
--             exists[item] = true
--             table.insert(result, item)
--         end
--     end
--     table.sort(result)
--     return result
-- end

-- local function get_module_files(base_dir, mod_name)
--     local files = {}

--     -- Backward compatible: src/em_driver/<mod>.c
--     local flat_file = path.join(base_dir, mod_name .. ".c")
--     if os.isfile(flat_file) then
--         table.insert(files, flat_file)
--     end

--     -- Preferred layout: src/em_driver/<mod>/*.c
--     local mod_dir = path.join(base_dir, mod_name)
--     if os.isdir(mod_dir) then
--         local sub_files = os.files(path.join(mod_dir, "**.c"))
--         for _, f in ipairs(sub_files) do
--             table.insert(files, f)
--         end
--     end

--     return dedup_and_sort(files)
-- end

-- local function get_supported_modules()
--     local basedir = path.join(os.projectdir(), "src", "em_driver")
--     local modules = {}

--     -- Flat layout modules: src/em_driver/<mod>.c
--     local flat_files = os.files(path.join(basedir, "*.c"))
--     for _, f in ipairs(flat_files) do
--         local mod = path.basename(f)
--         if mod then
--             table.insert(modules, mod)
--         end
--     end

--     -- Folder layout modules: src/em_driver/<mod>/**/*.c
--     local subdirs = os.dirs(path.join(basedir, "*"))
--     for _, dir in ipairs(subdirs) do
--         local rel = path.relative(dir, basedir)
--         -- Only keep first-level folders as module names
--         if rel and not rel:find("[/\\]") then
--             local has_c_files = #os.files(path.join(dir, "**.c")) > 0
--             if has_c_files then
--                 table.insert(modules, rel)
--             end
--         end
--     end

--     return dedup_and_sort(modules)
-- end

-- local function define_em_driver(name, default_config)
--     target(name)
--         set_kind("object")
--         add_deps("libca.em_base")
--         -- Keep includes stable for nested driver directories.
--         add_includedirs("$(projectdir)/src", {public = true})
--         add_includedirs("$(projectdir)/src/em_base", {public = true})
--         add_includedirs("$(projectdir)/src/em_driver", {public = true})
--         -- 根据配置加源码
--         on_load(function (target)
--             -- 1. 获取配置参数
--             local config = target:get("config") or default_config or {}
--             -- print(">>> [DEBUG] config type:", type(config))
--             -- print(">>> [DEBUG] config content:", config) 
--             local is_filter_enabled = config.enable_filter ~= false
--             local wanted_modules = config.drivers or {}
            
--             local files_to_compile = {}
            
--             -- 获取全局配置，看是否要显示驱动加载的详细信息
--             -- local show_info = has_config("show_driver_info") -- 已经封装到log里了

--             -- 2. 决定要扫描哪些文件
--             if not is_filter_enabled then
--                 -- 【调试模式】：扫描目录下所有 .c 文件
--                 log.debug("Filter disabled. Scanning all files.")
--                 -- 递归扫描，支持每个驱动单独子目录
--                 local all_files = os.files(path.join(os.scriptdir(), "**.c"))
--                 for _, f in ipairs(all_files) do
--                     table.insert(files_to_compile, f)
--                 end
                
--             else
--                 -- 【生产模式】：只扫描用户指定的模块
--                 -- 获取当前脚本的目录，确保路径准确
--                 local base_dir = os.scriptdir() 
                
--                 for _, mod_name in ipairs(wanted_modules) do
--                     local module_files = get_module_files(base_dir, mod_name)
--                     if #module_files > 0 then
--                         for _, f in ipairs(module_files) do
--                             table.insert(files_to_compile, f)
--                         end
--                     else
--                         -- 可选：如果用户指定了一个不存在的驱动，给个提示
--                         log.warn("module '" .. mod_name .. "' not found, skipping.")
--                     end
--                 end
--             end

--             files_to_compile = dedup_and_sort(files_to_compile)
            
--             -- 3. 批量添加筛选后的文件
--             if #files_to_compile > 0 then
--                 target:add("files", files_to_compile)
                
--                 -- 打印结果
--                 local enabled_list = {}
--                 for _, f in ipairs(files_to_compile) do
--                     table.insert(enabled_list, path.relative(f, os.scriptdir()))
--                 end
--                 enabled_list = dedup_and_sort(enabled_list)
                
--                 log.info("enabled modules (" .. #enabled_list .. "):")
--                 if #enabled_list > 0 then
--                     print("- " .. table.concat(enabled_list, ", "))
--                 end
--             else
--                 log.warn("No modules enabled!")
--             end
--         end)
-- end

-- define_em_driver("libca.em_driver", {enable_filter = true})
-- define_em_driver("libca.em_driver.all", {enable_filter = false})

-- target("libca.test-em_driver")
--     set_kind("static")
--     add_deps("libca.em_driver.all")

-- local function print_supported_modules()
--     local modules = get_supported_modules()
--     if #modules == 0 then
--         print("[libca.em.driver] no module found in " .. os.scriptdir())
--         return
--     end
--     print("[libca.em.driver] supported modules:")
--     for _, mod in ipairs(modules) do
--         print("  - " .. mod)
--     end
-- end

-- -- task to let user list modules via `xmake show-em-drivers`
-- task("show-em-drivers")
--     set_category("utils")
--     set_menu({
--         usage = "xmake show-em-drivers",
--         description = "Print supported em driver modules",
--     })
--     on_run(function ()
--         print_supported_modules()
--     end)

-- rule("libca.em.driver")
--     on_load(function (target)
--         -- print supported modules when rule is loaded
--         print_supported_modules()
--     end)

