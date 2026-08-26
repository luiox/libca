-- libca.i18n：嵌入式 CLI 消息国际化。
--
-- 组成：
--   1. rule("libca.i18n.embed-lang")：构建期把 translations/*.lang 生成为
--      constexpr 翻译表头（嵌入二进制，单 exe 分发零文件依赖）。各 target 经
--      add_files("translations/*.lang", {rules = "libca.i18n.embed-lang"}) 启用。
--      一个 target 一组翻译。
--   2. target("libca_i18n")：运行时（注册/查找/回退/占位符替换）。
--   3. target("libca_i18n_unittest")：含嵌入翻译 fixture 的端到端覆盖。

-- .lang 格式（UTF-8 properties 风格）：
--   # 注释
--   ns.path.key=译文，首个 = 分割，值两端空白剥离，内嵌 = 合法
--   占位符 {0} {1} …（可乱序/重复）
-- 构建期校验：key 须匹配 ^[a-z0-9_]+(\.[a-z0-9_]+)*$；同文件重复 key 报错；
-- zh_CN 必须存在且为超集（其它语言的 key 必须在 zh_CN 中出现）——zh_CN 是
-- 唯一事实源，运行时未命中其它语言回退 zh_CN。
rule("libca.i18n.embed-lang")
    set_extensions(".lang")
    add_orders("libca.i18n.embed-lang", "c++.build.modules.builder")
    on_load(function (target)
        -- 生成头为 UTF-8（含 CJK 字面量），MSVC 必须显式按 UTF-8 编译。
        if is_plat("windows") then
            target:add("cxflags", "/utf-8", {tools = "cl"})
        end
    end)
    on_preparecmd_file(function (target, batchcmds, sourcefile, opt)
        local langdir = path.directory(sourcefile)
        local langfiles = os.files(path.join(langdir, "*.lang"))
        assert(langfiles and #langfiles > 0, "no .lang files beside " .. sourcefile)
        table.sort(langfiles)

        -- 解析全部 .lang：langs[文件] = {{key,value},...}
        local langs = {}
        local seen_zh = false
        for _, file in ipairs(langfiles) do
            local name = path.basename(file)
            assert(name:match("^[a-z]+_[A-Z][A-Za-z]*$"),
                   "locale file name must be xx_YY.lang, got: " .. name)
            if name == "zh_CN" then seen_zh = true end
            local entries = {}
            local used = {}
            for line in io.readfile(file):gmatch("[^\r\n]+") do
                line = line:match("^%s*(.-)%s*$")
                if #line > 0 and line:sub(1, 1) ~= "#" then
                    local eq = line:find("=", 1, true)
                    assert(eq, "malformed line (expected key=value): " .. line)
                    local key = line:sub(1, eq - 1)
                    local val = line:sub(eq + 1)
                    assert(key:match("^[a-z0-9_.]+$") and not key:match("%.%.") and
                               key:sub(1, 1) ~= "." and key:sub(-1) ~= ".",
                           "invalid i18n key: " .. key)
                    assert(used[key] == nil, "duplicate i18n key in " .. name .. ".lang: " .. key)
                    used[key] = true
                    table.insert(entries, {key = key, val = val})
                end
            end
            table.sort(entries, function (a, b) return a.key < b.key end)
            langs[name] = entries
        end
        assert(seen_zh, "zh_CN.lang is required (fallback language): " .. langdir)

        -- 完整性：其它语言 key 必须是 zh_CN 的子集。
        local zhkeys = {}
        for _, e in ipairs(langs["zh_CN"]) do zhkeys[e.key] = true end
        for name, entries in pairs(langs) do
            if name ~= "zh_CN" then
                for _, e in ipairs(entries) do
                    assert(zhkeys[e.key],
                           "key missing in zh_CN.lang (source of truth): " .. e.key ..
                           " (from " .. name .. ".lang)")
                end
            end
        end

        -- .lang 值内联转义：\n → 换行，\\ → 反斜杠，其余 \x → x。
        -- （多行文案靠 \n 承载，文件本身保持单行。）
        local function unescape(s)
            return (s:gsub("\\(.)", function (c)
                if c == "n" then return "\n" end
                return c
            end))
        end

        -- C++ 字符串字面量转义：\、" 与换行。
        local function cppstr(s)
            return '"' .. s:gsub('[\\"]', "\\%1"):gsub("\n", "\\n") .. '"'
        end

        -- 符号后缀：target 名中的非标识符字符归一为 _。
        local suffix = target:name():gsub("[^%w]", "_")

        local generated = {
            "// Generated from translations/*.lang by rule libca.i18n.embed-lang. Do not edit.",
            "#pragma once",
            "",
            "#include <cstddef>",
            "",
            '#include "libca/i18n/i18n.hpp"',
            "",
        }
        for _, file in ipairs(langfiles) do
            local name = path.basename(file)
            table.insert(generated,
                         "constexpr ca::i18n::Entry kI18nEntries_" .. suffix .. "_" .. name .. "[] = {")
            for _, e in ipairs(langs[name]) do
                table.insert(generated,
                             "    {" .. cppstr(e.key) .. ", " .. cppstr(unescape(e.val)) .. "},")
            end
            table.insert(generated, "};")
            table.insert(generated, "")
        end
        table.insert(generated,
                     "constexpr ca::i18n::LangTable kI18nLangTables_" .. suffix .. "[] = {")
        for _, file in ipairs(langfiles) do
            local name = path.basename(file)
            table.insert(generated,
                         "    {" .. cppstr(name) .. ", kI18nEntries_" .. suffix .. "_" .. name ..
                         ", sizeof(kI18nEntries_" .. suffix .. "_" .. name .. ") / sizeof(kI18nEntries_" ..
                         suffix .. "_" .. name .. "[0])},")
        end
        table.insert(generated, "};")
        table.insert(generated, "")
        table.insert(generated,
                     "constexpr ca::i18n::TableSet kI18nTableSet_" .. suffix ..
                     " = {kI18nLangTables_" .. suffix ..
                     ", sizeof(kI18nLangTables_" .. suffix .. ") / sizeof(kI18nLangTables_" ..
                     suffix .. "[0])};")
        table.insert(generated, "")

        local headerdir = path.join(target:autogendir(), "rules", "libca_i18n", "embed-lang")
        if not os.isdir(headerdir) then
            os.mkdir(headerdir)
        end
        target:add("includedirs", headerdir, {public = true})

        local headerfile =
            path.join(headerdir, "i18n_" .. suffix .. "_translations.generated.hpp")
        local content = table.concat(generated, "\n")
        local existing = os.isfile(headerfile) and io.readfile(headerfile) or nil
        if existing ~= content then
            io.writefile(headerfile, content)
        end
        batchcmds:add_depfiles(sourcefile)
        for _, file in ipairs(langfiles) do
            batchcmds:add_depfiles(file)
        end
        batchcmds:set_depmtime(os.mtime(headerfile))
        batchcmds:set_depcache(target:dependfile(headerfile))
    end)

-- header-only：注册表用 function-local static，静态库依赖无下游链接负担。
target("libca_i18n")
    set_kind("headeronly")
    set_group("libs")

    add_deps("libca_core", "libca_str")
    add_headerfiles("src/(libca/i18n/*.hpp)")
    add_includedirs("src", {public = true})

-- 运行时单测 + 规则端到端：本 target 自身嵌入一组翻译 fixture，
-- 覆盖 tr 回退链 / trf 占位符 / env 与 --lang 解析 / 重复 key 覆盖语义。
target("libca_i18n_unittest")
    set_kind("binary")
    set_default(false)
    set_group("libs/test")

    add_deps("libca_i18n")
    add_packages("gtest")
    add_files("unittest/main.cpp")
    add_files("unittest/*_test.cpp")
    add_files("unittest/translations/*.lang", {rules = "libca.i18n.embed-lang"})
    add_includedirs("src")
    -- header-only 依赖不自动传递链接，binary 显式补齐。
    add_links("libca_core", "libca_str")
    if is_plat("windows") then
        add_cxflags("/utf-8", {tools = "cl"})
    end
