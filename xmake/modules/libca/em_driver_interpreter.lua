-- libca.em_driver_interpreter: interpret data-driven driver manifests

local inject = import("libca.em_inject")

local function _extract_brace_block(text, from_pos)
    local start_pos = text:find("{", from_pos, true)
    if not start_pos then
        return nil
    end

    local level = 0
    for i = start_pos, #text do
        local ch = text:sub(i, i)
        if ch == "{" then
            level = level + 1
        elseif ch == "}" then
            level = level - 1
            if level == 0 then
                return text:sub(start_pos, i)
            end
        end
    end
    return nil
end

local function _deserialize_static_manifest(content, driver_name)
    if not content:match("^%s*return%s+function%s*%(%s*ctx%s*%)") then
        raise("libca.em: em_driver.%s manifest must return function(ctx)", tostring(driver_name))
    end

    local body = content:match("^%s*return%s+function%s*%(%s*ctx%s*%)%s*(.-)%s*end%s*$")
    if type(body) ~= "string" or body == "" then
        raise("libca.em: em_driver.%s manifest function(ctx) body invalid", tostring(driver_name))
    end

    local table_expr
    local config_anchor = body:find("local%s+config%s*=")
    if config_anchor then
        table_expr = _extract_brace_block(body, config_anchor)
    else
        local return_anchor = body:find("return%s+")
        if return_anchor then
            table_expr = _extract_brace_block(body, return_anchor)
        end
    end

    if type(table_expr) ~= "string" or table_expr == "" then
        raise("libca.em: em_driver.%s manifest function(ctx) must return static table", tostring(driver_name))
    end

    local manifest = string.deserialize(table_expr)
    if type(manifest) ~= "table" then
        raise("libca.em: em_driver.%s manifest function(ctx) must return table", tostring(driver_name))
    end
    return manifest
end

local function _load_driver_manifest(state, driver_name)
    local em_driver_root = path.join(state.root, "src", "em_driver")
    local manifest_path = path.join(em_driver_root, driver_name, driver_name .. ".lua")

    if not os.isfile(manifest_path) then
        raise("libca.em: em_driver.%s manifest not found: %s", tostring(driver_name), manifest_path)
    end

    local content = io.readfile(manifest_path)
    if type(content) ~= "string" or content == "" then
        raise("libca.em: em_driver.%s manifest read failed: %s", tostring(driver_name), manifest_path)
    end

    if content:sub(1, 3) == "\239\187\191" then
        content = content:sub(4)
    end

    local manifest = _deserialize_static_manifest(content, driver_name)

    if type(manifest.name) ~= "string" or manifest.name == "" then
        raise("libca.em: em_driver.%s manifest.name must be non-empty string", tostring(driver_name))
    end
    if type(manifest.dir) ~= "string" or manifest.dir == "" then
        raise("libca.em: em_driver.%s manifest.dir must be non-empty string", tostring(driver_name))
    end
    if type(manifest.src) ~= "table" or #manifest.src == 0 then
        raise("libca.em: em_driver.%s.src must be non-empty list(table)", tostring(driver_name))
    end

    local cfg_map = manifest.port_config
    if type(cfg_map) ~= "table" then
        raise("libca.em: em_driver.%s.port_config must be table", tostring(driver_name))
    end
    if type(cfg_map.mode) ~= "table" then
        raise("libca.em: em_driver.%s.port_config.mode must be table", tostring(driver_name))
    end

    return manifest
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

local function _inject_port_config(target, state, manifest, opts)
    local cfg_map = manifest.port_config

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

        local selected = opts[cfg_name]
        if cfg_name == "mode" and selected == nil then
            raise("libca.em: em_driver.%s.mode is required", tostring(manifest.name))
        end
        if selected == nil then
            selected = cfg.default
        end
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
    _inject_port_config(target, state, manifest, opts)
    _inject_ports(target, state, manifest, opts)
end
