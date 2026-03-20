-- libca.em.entry: import facade for setup/add_libs APIs

local core = import("libca.em_core")
local registry = import("libca.em_registry")

function setup(target, opts)
    core.setup(target, opts, registry)
end

function add_libs(target, name, opts)
    core.add_libs(target, name, opts, registry)
end

function get_state(target)
    return core.get_state(target)
end

function register_module(name, spec)
    registry.register_module(name, spec)
end

function register_driver(name, spec)
    registry.register_driver(name, spec)
end

-- Backward-compatible single-call entry for early demos.
function use(target, opts)
    opts = opts or {}
    setup(target, {root = opts.root})

    if opts.em_base then
        add_libs(target, "em_base", opts.em_base)
    end

    local drivers = opts.drivers or {}
    if #drivers > 0 then
        local em_driver_opts = {}
        for _, d in ipairs(drivers) do
            if type(d) ~= "table" or type(d.name) ~= "string" then
                raise("libca.em: drivers item must be table with field 'name'")
            end

            local driver_name = d.name
            local driver_opts = {}
            for k, v in pairs(d) do
                if k ~= "name" then
                    driver_opts[k] = v
                end
            end
            em_driver_opts[driver_name] = driver_opts
        end
        add_libs(target, "em_driver", em_driver_opts)
    end
end
