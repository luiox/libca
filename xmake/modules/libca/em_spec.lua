-- libca.em_spec: reusable spec factories for modules and drivers

local inject = import("libca.em_inject")

local function _as_list(v, field_name)
    if v == nil then
        return {}
    end
    if type(v) == "string" then
        return {v}
    end
    if type(v) ~= "table" then
        raise("libca.em: %s must be list(table) or string", tostring(field_name))
    end
    return v
end

local function _copy_table(src)
    local dst = {}
    for k, v in pairs(src or {}) do
        dst[k] = v
    end
    return dst
end

function make_simple_module(cfg)
    cfg = cfg or {}
    local deps = cfg.deps or {}
    local include_src = cfg.include_src ~= false
    local files = cfg.files or {}

    return {
        deps = deps,
        inject = function (target, state)
            local src_root = path.join(state.root, "src")
            if include_src then
                inject.add_include(target, state, src_root)
            end

            local abs_files = {}
            for _, rel in ipairs(files) do
                table.insert(abs_files, path.join(src_root, rel))
            end
            inject.add_files(target, state, abs_files)
        end
    }
end

function make_port_mode_driver(cfg)
    cfg = cfg or {}
    local driver_name = cfg.driver_name or "driver"
    local mode_opt = cfg.mode_opt or "mode"
    local port_opt = cfg.port_opt or "port"

    return {
        inject = function (target, state, opts)
            opts = opts or {}

            local src_root = path.join(state.root, "src")
            local driver_dir = path.join(src_root, cfg.rel_dir)
            local mode = opts[mode_opt] or "extern"

            if mode ~= "extern" and mode ~= "dynamic" then
                raise("libca.em: em_driver.%s.%s must be 'extern' or 'dynamic'", driver_name, mode_opt)
            end

            inject.add_include(target, state, src_root)
            inject.add_file(target, state, path.join(driver_dir, cfg.source))

            if mode == "dynamic" then
                inject.add_define(target, state, cfg.define_dynamic)
                return
            end

            inject.add_define(target, state, cfg.define_extern)

            local ports = _as_list(opts[port_opt], "em_driver." .. driver_name .. "." .. port_opt)
            if #ports > 0 then
                for _, p in ipairs(ports) do
                    if type(p) ~= "string" then
                        raise("libca.em: em_driver.%s.%s item must be string path", driver_name, port_opt)
                    end
                    inject.add_file(target, state, inject.to_abs(os.projectdir(), p))
                end
                return
            end

            inject.add_file(target, state, path.join(driver_dir, cfg.default_port_source))
        end
    }
end

function make_driver_module(cfg)
    cfg = cfg or {}
    local deps = cfg.deps or {"em_base"}
    local default_driver_opts = _copy_table(cfg.default_driver_opts)

    return {
        deps = deps,
        inject = function (target, state, opts, reg)
            opts = opts or {}
            for driver_name, driver_opts in pairs(opts) do
                local spec = reg.get_driver(driver_name)
                if not spec then
                    raise("libca.em: unsupported em_driver '%s'", tostring(driver_name))
                end

                local merged_opts = _copy_table(default_driver_opts)
                for k, v in pairs(driver_opts or {}) do
                    merged_opts[k] = v
                end
                spec.inject(target, state, merged_opts)
            end
        end
    }
end