if path.absolute(os.projectdir()) == path.absolute(os.scriptdir()) then
    set_project("libca-demo")
    set_version("0.0.1")
    set_xmakever("2.8.3")
end
if is_plat("windows") then
    -- Xmake maps c99 to /TP for MSVC; C11 keeps these C99-compatible sources in C mode.
    set_languages("c11")
else
    set_languages("c99")
end

add_rules("mode.debug", "mode.release")

if is_plat("windows") then
    add_cflags("/utf-8")
end

-- 模拟用户工程：只通过 import 模块接入 libca 源码包
add_moduledirs(path.join(os.scriptdir(), "..", "..", "xmake", "modules"))

target("demo_led_extern")
    set_kind("binary")
    add_files("app/main.c")
    on_load(function (target)
        local em = import("libca.em")
        em.setup(target, {
            root = path.join(os.scriptdir(), "..", "..")
        })

        em.add_libs(target, {
            em_driver = {
                led = {
                    mode = "extern",
                    port = {path.join(os.scriptdir(), "board", "port_led.c")}
                }
            },
            em_base = {}
        })
    end)

target("demo_led_dynamic")
    set_kind("binary")
    add_files("app/main_dynamic.c")
    on_load(function (target)
        local em = import("libca.em")
        em.setup(target, {
            root = path.join(os.scriptdir(), "..", "..")
        })

        em.add_libs(target, {
            em_driver = {
                led = {
                    mode = "dynamic"
                }
            },
            em_base = {}
        })
    end)

target("demo_led_no_port")
    set_kind("binary")
    add_files("app/main_dynamic.c")
    on_load(function (target)
        local em = import("libca.em")
        em.setup(target, {
            root = path.join(os.scriptdir(), "..", "..")
        })

        em.add_libs(target, {
            em_driver = {
                led = {
                    mode = "dynamic"
                }
            },
            em_base = {}
        })
    end)

target("demo_module_batch")
    set_kind("binary")
    add_files("app/main_modules.c")
    on_load(function (target)
        local em = import("libca.em")
        em.setup(target, {
            root = path.join(os.scriptdir(), "..", "..")
        })
        em.add_libs(target, {
            em_component = true,
            em_util = true,
            em_base = true
        })
    end)

target("demo_driver_manifests_check")
    set_kind("binary")
    add_files("app/main.c")
    on_load(function (target)
        local root = path.join(os.scriptdir(), "..", "..")
        local driver_root = path.join(root, "libca.em", "src", "em_driver")

        local function extract_brace_block(text, from_pos)
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

        local function parse_manifest(content, driver_name)
            if not content:match("^%s*return%s+function%s*%(%s*ctx%s*%)") then
                raise("demo check: manifest must return function(ctx): %s", driver_name)
            end

            local body = content:match("^%s*return%s+function%s*%(%s*ctx%s*%)%s*(.-)%s*end%s*$")
            if type(body) ~= "string" or body == "" then
                raise("demo check: manifest body invalid: %s", driver_name)
            end

            local table_expr
            local config_anchor = body:find("local%s+config%s*=")
            if config_anchor then
                table_expr = extract_brace_block(body, config_anchor)
            else
                local return_anchor = body:find("return%s+")
                if return_anchor then
                    table_expr = extract_brace_block(body, return_anchor)
                end
            end

            if type(table_expr) ~= "string" or table_expr == "" then
                raise("demo check: manifest static table missing: %s", driver_name)
            end

            local cfg = string.deserialize(table_expr)
            if type(cfg) ~= "table" then
                raise("demo check: manifest static table invalid: %s", driver_name)
            end
            return cfg
        end

        local unsupported_drivers = {
            icm20948 = true
        }
        for _, dir in ipairs(os.dirs(path.join(driver_root, "*"))) do
            local driver_name = path.basename(dir)
            local manifest = path.join(dir, driver_name .. ".lua")
            if not os.isfile(manifest) then
                if not unsupported_drivers[driver_name] then
                    raise("demo check: missing driver manifest %s", manifest)
                end
            else
                local content = io.readfile(manifest)
                if type(content) ~= "string" or content == "" then
                    raise("demo check: manifest read failed %s", manifest)
                end
                if content:sub(1, 3) == "\239\187\191" then
                    content = content:sub(4)
                end

                local cfg = parse_manifest(content, driver_name)

                if type(cfg.name) ~= "string" or cfg.name == "" then
                    raise("demo check: manifest.name invalid: %s", manifest)
                end
                if type(cfg.dir) ~= "string" or cfg.dir == "" then
                    raise("demo check: manifest.dir invalid: %s", manifest)
                end
                if type(cfg.src) ~= "table" or #cfg.src == 0 then
                    raise("demo check: manifest.src invalid: %s", manifest)
                end
                if type(cfg.port_config) ~= "table" or type(cfg.port_config.mode) ~= "table" then
                    raise("demo check: manifest.port_config.mode invalid: %s", manifest)
                end
            end
        end

        local em = import("libca.em")
        em.setup(target, {
            root = root
        })
        em.add_libs(target, {
            em_driver = {
                led = {
                    mode = "extern",
                    port = {path.join(os.scriptdir(), "board", "port_led.c")}
                }
            },
            em_base = {}
        })
    end)

target("check_em_contract")
    set_kind("phony")
    set_default(false)
    on_run(function ()
        local em = import("libca.em")
        local root = path.join(os.scriptdir(), "..", "..")

        local function new_fake_target(name)
            local result = {
                _name = name,
                _values = {}
            }
            function result:name()
                return self._name
            end
            function result:add(kind, value)
                self._values[kind] = self._values[kind] or {}
                table.insert(self._values[kind], value)
            end
            function result:set(kind, value)
                if kind == "load_after" then
                    self._load_after = value
                else
                    self._values[kind] = value
                end
            end
            function result:script(kind)
                if kind == "load_after" then
                    return self._load_after
                end
                return nil
            end
            return result
        end

        local function finalize(target)
            if type(target._load_after) ~= "function" then
                raise("contract check: target '%s' has no finalizer", target:name())
            end
            target._load_after(target)
        end

        local function expect_error(label, expected, action)
            local caught
            try {
                action,
                catch {
                    function (errors)
                        caught = tostring(errors)
                    end
                }
            }
            if not caught or not caught:find(expected, 1, true) then
                raise("contract check: %s did not report '%s'", label, expected)
            end
        end

        local expected_modules = {
            "em_base",
            "em_bus",
            "em_component",
            "em_crypto",
            "em_driver",
            "em_dstream",
            "em_format",
            "em_log",
            "em_motion",
            "em_mpool",
            "em_ota",
            "em_platform",
            "em_protocol",
            "em_shell",
            "em_test",
            "em_util"
        }
        local actual_modules = em.list_modules()
        if table.concat(actual_modules, ",") ~= table.concat(expected_modules, ",") then
            raise("contract check: built-in module catalog mismatch")
        end

        local valid = new_fake_target("valid-order-independent")
        em.setup(valid, {root = root})
        em.add_libs(valid, "em_protocol")
        em.add_libs(valid, {
            em_component = true,
            em_base = {memory_util = "custom"},
            em_util = true
        })
        em.add_libs(valid, "em_base", {memory_util = "std"})
        finalize(valid)

        local valid_state = em.get_state(valid)
        for _, name in ipairs({"em_base", "em_component", "em_protocol", "em_util"}) do
            if not (valid_state.modules[name] and valid_state.modules[name].enable) then
                raise("contract check: module '%s' was not enabled", name)
            end
        end
        for _, define in ipairs(valid._values.defines or {}) do
            if define == "USE_CUSTOM_MEMORY_UTIL_IMPL=1" then
                raise("contract check: repeated module options did not keep the latest value")
            end
        end
        if #(valid._values.includedirs or {}) ~= 1 then
            raise("contract check: src_root include directory was not deduplicated")
        end

        local catalog = new_fake_target("complete-catalog")
        em.setup(catalog, {root = root})
        local catalog_request = {}
        for _, name in ipairs(expected_modules) do
            catalog_request[name] = true
        end
        em.add_libs(catalog, catalog_request)
        finalize(catalog)

        em.register_driver("contract_override", {
            handle = function (target)
                target:add("defines", "CONTRACT_OVERRIDE_APPLIED=1")
            end
        })
        local override = new_fake_target("driver-override")
        em.setup(override, {root = root})
        em.add_libs(override, {
            em_driver = {contract_override = true},
            em_base = true
        })
        finalize(override)
        if table.concat(override._values.defines or {}, ","):find("CONTRACT_OVERRIDE_APPLIED=1", 1, true) == nil then
            raise("contract check: driver override without manifest was not applied")
        end

        expect_error("setup options", "options must be table", function ()
            em.setup(new_fake_target("invalid-setup-options"), "invalid")
        end)

        expect_error("setup root", "root does not contain libca.em/src", function ()
            em.setup(new_fake_target("invalid-setup-root"), {root = os.scriptdir()})
        end)

        expect_error("missing dependency", "missing dependency", function ()
            local target = new_fake_target("missing-dependency")
            em.setup(target, {root = root})
            em.add_libs(target, "em_protocol")
            finalize(target)
        end)

        expect_error("unsupported module", "unsupported module 'em_eimui'", function ()
            local target = new_fake_target("unsupported-module")
            em.setup(target, {root = root})
            em.add_libs(target, "em_eimui")
        end)

        expect_error("driver dependency", "dependency=em_util", function ()
            local target = new_fake_target("driver-dependency")
            em.setup(target, {root = root})
            em.add_libs(target, {
                em_base = true,
                em_driver = {
                    tofxxf = {mode = "dynamic"}
                }
            })
            finalize(target)
        end)

        print("libca.em module contract: ok")
    end)
