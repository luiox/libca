set_project("libca-demo")
set_version("0.0.1")
set_xmakever("2.8.3")
set_languages("c99")

add_rules("mode.debug", "mode.release")

-- 模拟用户工程：只通过 import 模块接入 libca 源码包
add_moduledirs(path.join(os.scriptdir(), "..", "xmake", "modules"))

target("demo_led_extern")
    set_kind("binary")
    add_files("app/main.c")
    on_load(function (target)
        local em = import("libca.em")
        em.setup(target, {
            root = path.join(os.scriptdir(), "..")
        })

        em.add_libs(target, "em_base")
        em.add_libs(target, "em_driver", {
            led = {
                mode = "extern",
                port = {path.join(os.scriptdir(), "board", "port_led.c")}
            }
        })
    end)

target("demo_led_dynamic")
    set_kind("binary")
    add_files("app/main_dynamic.c")
    on_load(function (target)
        local em = import("libca.em")
        em.setup(target, {
            root = path.join(os.scriptdir(), "..")
        })

        em.add_libs(target, "em_base")
        em.add_libs(target, "em_driver", {
            led = {
                mode = "dynamic"
            }
        })
    end)

target("demo_led_no_port")
    set_kind("binary")
    add_files("app/main_dynamic.c")
    on_load(function (target)
        local em = import("libca.em")
        em.setup(target, {
            root = path.join(os.scriptdir(), "..")
        })

        em.add_libs(target, "em_base")
        em.add_libs(target, "em_driver", {
            led = {
                mode = "dynamic"
            }
        })
    end)

target("demo_driver_manifests_check")
    set_kind("binary")
    add_files("app/main.c")
    on_load(function (target)
        local root = path.join(os.scriptdir(), "..")
        local driver_root = path.join(root, "src", "em_driver")

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

        for _, dir in ipairs(os.dirs(path.join(driver_root, "*"))) do
            local driver_name = path.basename(dir)
            local manifest = path.join(dir, driver_name .. ".lua")
            if not os.isfile(manifest) then
                raise("demo check: missing driver manifest %s", manifest)
            end

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

        local em = import("libca.em")
        em.setup(target, {
            root = root
        })
        em.add_libs(target, "em_base")
        em.add_libs(target, "em_driver", {
            led = {
                mode = "extern",
                port = {path.join(os.scriptdir(), "board", "port_led.c")}
            }
        })
    end)
