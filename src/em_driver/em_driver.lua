-- 嵌入式驱动库

-- 驱动注册表：集中管理每个 driver 的源码与模式宏。
local em_driver_registry = {
	["libca.em_driver.led"] = {
		driver_dir = path.join(os.scriptdir(), "led"),
		core_files = {"led.c"},
		default_port_file = "port_led.c",
		extern_define = "LIBCA_LED_PORT_MODE=1",
		dynamic_define = "LIBCA_LED_PORT_MODE=2"
	}
}

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
local function em_driver_apply_spec(target, libname, opts, mode)
	local spec = em_driver_registry[libname]
	if not spec then
		raise("m_add_libs: unsupported lib '%s'", tostring(libname))
	end

	local driver_dir = spec.driver_dir
	local core_files = spec.core_files or {}
	local port_files = em_driver_normalize_port_files(libname, opts.port)

	local src_root = path.join(driver_dir, "..", "..")
	target:add("includedirs", src_root)

	for _, f in ipairs(core_files) do
		target:add("files", path.join(driver_dir, f))
	end

	if mode == "dynamic" then
		target:add("defines", spec.dynamic_define)
		return
	end

	target:add("defines", spec.extern_define)

	if #port_files > 0 then
		target:add("files", port_files)
	elseif spec.default_port_file then
		-- extern mode without user port list: use built-in weak default port
		target:add("files", path.join(driver_dir, spec.default_port_file))
	end
end

-- 对外 API：不使用 add_rules，直接向目标注入 em_driver 源码。
local function m_add_libs(target, libname, opts)
	if type(target) ~= "table" or type(target.add) ~= "function" then
		raise("m_add_libs: first argument must be a target object")
	end

	if type(libname) ~= "string" or libname == "" then
		raise("m_add_libs: second argument must be a non-empty lib name")
	end

	opts = opts or {}
	local mode = opts.mode or "extern"
	if mode ~= "extern" and mode ~= "dynamic" then
		raise("m_add_libs: invalid mode '%s', expected 'extern' or 'dynamic'", tostring(mode))
	end

	if opts.port ~= nil and type(opts.port) ~= "table" then
		raise("m_add_libs: 'port' must be a list(table), for example {\"1.c\", \"2.c\"}")
	end

	em_driver_apply_spec(target, libname, opts, mode)
end

-- LED 驱动规则示例
rule("libca.em_driver.led")
	on_load(function (target)
		local rule_name = "libca.em_driver.led"
		local opts, mode = em_driver_get_rule_options(target, rule_name)
		m_add_libs(target, rule_name, {
			mode = mode,
			port = opts.port
		})
	end)
rule_end()
