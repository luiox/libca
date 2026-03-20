-- libca.em: source-package manager (import entry)

local _states = {}

local function _target_key(target)
    if type(target.name) == "function" then
        return target:name()
    end
    return tostring(target)
end

local function _to_abs(root, p)
    if not p then
        return nil
    end
    return path.is_absolute(p) and p or path.absolute(p, root)
end

local function _new_state(root)
    return {
        root = root,
        em_base = {enable = false},
        em_driver = {enable = false},
        injected = {
            files = {},
            includedirs = {},
            defines = {}
        }
    }
end

local function _add_unique(target, bucket, key, adder, value)
    if bucket[key] then
        return
    end
    bucket[key] = true
    adder(target, value)
end

local function _add_file(target, state, f)
    _add_unique(target, state.injected.files, f, function (t, v)
        t:add("files", v)
    end, f)
end

local function _add_include(target, state, d)
    _add_unique(target, state.injected.includedirs, d, function (t, v)
        t:add("includedirs", v)
    end, d)
end

local function _add_define(target, state, d)
    _add_unique(target, state.injected.defines, d, function (t, v)
        t:add("defines", v)
    end, d)
end

local function _inject_em_base(target, state)
    local src_root = path.join(state.root, "src")
    local base_dir = path.join(src_root, "em_base")

    state.em_base.enable = true

    _add_include(target, state, src_root)
    _add_file(target, state, path.join(base_dir, "datatype.c"))
    _add_file(target, state, path.join(base_dir, "debug.c"))
    _add_file(target, state, path.join(base_dir, "compiler_compat.c"))
end

local function _normalize_port_list(port)
    if port == nil then
        return {}
    end
    if type(port) == "string" then
        return {port}
    end
    if type(port) ~= "table" then
        raise("libca.em: port must be list(table) or string")
    end
    return port
end

local function _inject_led(target, state, led_opts)
    led_opts = led_opts or {}

    local src_root = path.join(state.root, "src")
    local led_dir = path.join(src_root, "em_driver", "led")
    local mode = led_opts.mode or "extern"

    if mode ~= "extern" and mode ~= "dynamic" then
        raise("libca.em: em_driver.led.mode must be 'extern' or 'dynamic'")
    end

    _add_include(target, state, src_root)
    _add_file(target, state, path.join(led_dir, "led.c"))

    if mode == "dynamic" then
        _add_define(target, state, "LIBCA_LED_PORT_MODE=2")
        return
    end

    _add_define(target, state, "LIBCA_LED_PORT_MODE=1")

    local ports = _normalize_port_list(led_opts.port)
    if #ports > 0 then
        for _, p in ipairs(ports) do
            if type(p) ~= "string" then
                raise("libca.em: each em_driver.led.port item must be string path")
            end
            _add_file(target, state, _to_abs(os.projectdir(), p))
        end
    else
        _add_file(target, state, path.join(led_dir, "port_led.c"))
    end
end

function setup(target, opts)
    if type(target) ~= "table" or type(target.add) ~= "function" then
        raise("libca.em.setup: target object is required")
    end

    opts = opts or {}
    local root = _to_abs(os.projectdir(), opts.root or "")
    if not root or root == "" or not os.isdir(root) then
        raise("libca.em.setup: invalid root '%s'", tostring(opts.root))
    end

    _states[_target_key(target)] = _new_state(root)
end

function add_libs(target, name, opts)
    if type(target) ~= "table" or type(target.add) ~= "function" then
        raise("libca.em.add_libs: target object is required")
    end

    local state = _states[_target_key(target)]
    if not state then
        raise("libca.em.add_libs: call setup(target, {root = ...}) first")
    end

    opts = opts or {}

    if name == "em_base" then
        _inject_em_base(target, state)
        return
    end

    if name == "em_driver" then
        state.em_driver.enable = true

        -- em_driver always depends on em_base
        _inject_em_base(target, state)

        local led_opts = opts.led
        if led_opts then
            _inject_led(target, state, led_opts)
        end
        return
    end

    raise("libca.em.add_libs: unsupported module '%s'", tostring(name))
end

function get_state(target)
    return _states[_target_key(target)]
end
