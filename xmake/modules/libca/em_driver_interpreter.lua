-- libca.em_driver_interpreter: interpret data-driven driver manifests

local inject = import("libca.em_inject")

local function _list_to_set(items)
    local set = {}
    for _, item in ipairs(items or {}) do
        set[item] = true
    end
    return set
end

local function _collect_driver_sources(driver_dir)
    local excluded_prefix = _list_to_set({"test-", "example"})
    local result = {}

    for _, f in ipairs(os.files(path.join(driver_dir, "**.c"))) do
        local filename = path.filename(f)
        local excluded = false
        for prefix, _ in pairs(excluded_prefix) do
            if filename:sub(1, #prefix) == prefix then
                excluded = true
                break
            end
        end
        if not excluded then
            table.insert(result, path.relative(f, driver_dir))
        end
    end
    return result
end

local function _load_driver_manifest(state, driver_name)
    local em_driver_root = path.join(state.root, "src", "em_driver")
    local driver_dir = path.join(em_driver_root, driver_name)
    local manifest_path = path.join(em_driver_root, driver_name, driver_name .. ".lua")

    if not os.isfile(manifest_path) then
        manifest_path = path.join(em_driver_root, driver_name .. ".lua")
    end

    if os.isfile(manifest_path) then
        local content = io.readfile(manifest_path)
        if type(content) ~= "string" or content == "" then
            raise("libca.em: em_driver.%s manifest read failed: %s", tostring(driver_name), manifest_path)
        end

        if content:sub(1, 3) == "\239\187\191" then
            content = content:sub(4)
        end

        local expr = content:gsub("^%s*return%s+", "", 1)
        local manifest = string.deserialize(expr)
        if type(manifest) ~= "table" then
            raise("libca.em: em_driver.%s manifest must return table", tostring(driver_name))
        end
        manifest.name = manifest.name or driver_name
        manifest.dir = manifest.dir or driver_name
        manifest.src = manifest.src or {}
        manifest.default_port_src = manifest.default_port_src or {}
        return manifest
    end

    if not os.isdir(driver_dir) then
        raise("libca.em: em_driver.%s directory not found: %s", tostring(driver_name), driver_dir)
    end

    local src_list = _collect_driver_sources(driver_dir)
    if #src_list == 0 then
        raise("libca.em: em_driver.%s has no source files in %s", tostring(driver_name), driver_dir)
    end

    return {
        name = driver_name,
        dir = driver_name,
        src = src_list,
        default_port_src = {}
    }
end

local function _ensure_file(abs_path, driver_name)
    if not os.isfile(abs_path) then
        raise("libca.em: em_driver.%s source not found: %s", tostring(driver_name), abs_path)
    end
end

local function _inject_sources(target, state, manifest)
    local src_root = path.join(state.root, "src")
    local em_driver_root = path.join(src_root, "em_driver")
    local driver_name = manifest.name
    local driver_dir = path.join(em_driver_root, manifest.dir)

    if type(manifest.src) ~= "table" or #manifest.src == 0 then
        raise("libca.em: em_driver.%s.src must be non-empty list(table)", tostring(driver_name))
    end

    inject.add_include(target, state, src_root)

    for _, rel in ipairs(manifest.src) do
        if type(rel) ~= "string" then
            raise("libca.em: em_driver.%s.src item must be string", tostring(driver_name))
        end
        local abs = path.join(driver_dir, rel)
        _ensure_file(abs, driver_name)
        inject.add_file(target, state, abs)
    end
end

local function _inject_default_ports(target, state, manifest, opts)
    if opts.port ~= nil then
        return
    end

    local src_root = path.join(state.root, "src")
    local em_driver_root = path.join(src_root, "em_driver")
    local driver_name = manifest.name
    local driver_dir = path.join(em_driver_root, manifest.dir)

    local default_ports = manifest.default_port_src or {}
    if #default_ports == 0 then
        for _, f in ipairs(os.files(path.join(driver_dir, "port_*.c"))) do
            table.insert(default_ports, path.relative(f, driver_dir))
        end
    end

    for _, rel in ipairs(default_ports) do
        if type(rel) ~= "string" then
            raise("libca.em: em_driver.%s.default_port_src item must be string", tostring(driver_name))
        end
        local abs = path.join(driver_dir, rel)
        _ensure_file(abs, driver_name)
        inject.add_file(target, state, abs)
    end
end

local function _inject_port_config(target, state, manifest, opts)
    local cfg_map = manifest.port_config
    if type(cfg_map) ~= "table" then
        return
    end

    for cfg_name, cfg in pairs(cfg_map) do
        if type(cfg) ~= "table" then
            raise("libca.em: em_driver.%s.port_config.%s must be table", tostring(manifest.name), tostring(cfg_name))
        end
        if type(cfg.default) ~= "string" then
            raise("libca.em: em_driver.%s.port_config.%s.default must be string", tostring(manifest.name), tostring(cfg_name))
        end
        if type(cfg.values) ~= "table" then
            raise("libca.em: em_driver.%s.port_config.%s.values must be table", tostring(manifest.name), tostring(cfg_name))
        end

        local selected = opts[cfg_name] or cfg.default
        local define = cfg.values[selected]
        if type(define) ~= "string" or define == "" then
            raise("libca.em: em_driver.%s.%s invalid option '%s'", tostring(manifest.name), tostring(cfg_name), tostring(selected))
        end
        inject.add_define(target, state, define)
    end
end

local function _inject_ports(target, state, manifest, opts)
    local ports = opts.port
    if ports == nil then
        return
    end
    if type(ports) ~= "table" then
        raise("libca.em: em_driver.%s.port must be list(table)", tostring(manifest.name))
    end

    for _, p in ipairs(ports) do
        if type(p) ~= "string" then
            raise("libca.em: em_driver.%s.port item must be string path", tostring(manifest.name))
        end
        if not path.is_absolute(p) then
            raise("libca.em: em_driver.%s.port item must be absolute path: %s", tostring(manifest.name), p)
        end
        if not os.isfile(p) then
            raise("libca.em: em_driver.%s.port file not found: %s", tostring(manifest.name), p)
        end
        inject.add_file(target, state, p)
    end
end

function handle_driver(target, state, driver_name, opts)
    opts = opts or {}

    local manifest = _load_driver_manifest(state, driver_name)
    manifest.name = manifest.name or driver_name

    _inject_sources(target, state, manifest)
    _inject_default_ports(target, state, manifest, opts)
    _inject_port_config(target, state, manifest, opts)
    _inject_ports(target, state, manifest, opts)
end
