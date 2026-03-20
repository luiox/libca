-- libca em import entry (minimal demo version)

local function _to_abs(root, p)
    if not p then
        return nil
    end
    return path.is_absolute(p) and p or path.absolute(p, root)
end

local function _add_em_base(target, root)
    local src_root = path.join(root, "src")
    local base_dir = path.join(src_root, "em_base")

    target:add("includedirs", src_root)

    -- Minimal em_base set for led demo path.
    target:add("files", path.join(base_dir, "datatype.c"))
    target:add("files", path.join(base_dir, "debug.c"))
    target:add("files", path.join(base_dir, "compiler_compat.c"))
end

local function _add_led(target, root, opts)
    opts = opts or {}

    local src_root = path.join(root, "src")
    local led_dir = path.join(src_root, "em_driver", "led")

    local mode = opts.mode or "extern"
    if mode ~= "extern" and mode ~= "dynamic" then
        raise("libca.em.entry: invalid led mode '%s'", tostring(mode))
    end

    target:add("includedirs", src_root)
    target:add("files", path.join(led_dir, "led.c"))

    if mode == "dynamic" then
        target:add("defines", "LIBCA_LED_PORT_MODE=2")
        return
    end

    target:add("defines", "LIBCA_LED_PORT_MODE=1")

    local port = opts.port
    if port ~= nil and type(port) ~= "table" then
        raise("libca.em.entry: led port must be a list(table)")
    end

    if type(port) == "table" and #port > 0 then
        local files = {}
        for _, f in ipairs(port) do
            if type(f) ~= "string" then
                raise("libca.em.entry: each led port item must be a string path")
            end
            table.insert(files, _to_abs(os.projectdir(), f))
        end
        target:add("files", files)
    else
        -- extern mode fallback to built-in weak default port
        target:add("files", path.join(led_dir, "port_led.c"))
    end
end

function use(target, opts)
    if type(target) ~= "table" or type(target.add) ~= "function" then
        raise("libca.em.entry.use: target object is required")
    end

    opts = opts or {}
    local root = _to_abs(os.projectdir(), opts.root or "")
    if not root or not os.isdir(root) then
        raise("libca.em.entry.use: invalid root '%s'", tostring(opts.root))
    end

    _add_em_base(target, root)

    local drivers = opts.drivers or {}
    for _, d in ipairs(drivers) do
        if d.name == "led" then
            _add_led(target, root, d)
        else
            raise("libca.em.entry.use: unsupported driver '%s'", tostring(d.name))
        end
    end
end
