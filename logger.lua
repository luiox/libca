-- ============================================
-- 极简原生日志
-- 纯字符串拼接
-- ============================================

-- 1. ANSI 颜色定义
local ANSI_COLORS = {
    reset  = "\27[0m",
    dim    = "\27[90m",
    red    = "\27[31m",
    green  = "\27[32m",
    yellow = "\27[33m",
    cyan   = "\27[36m"
}

-- 2. 日志等级定义
local LOG_LEVELS = {
    DEBUG = 10,
    INFO  = 20,
    WARN  = 30,
    ERROR = 40
}

-- 全局默认等级
local GLOBAL_LEVEL = LOG_LEVELS.INFO

-- 模块白名单配置
local MODULE_CONFIGS = {}

-- 读取环境变量 XMAKE_LOG_DEBUG_MODULES (例如: core,network)
local env_modules = os.getenv("XMAKE_LOG_DEBUG_MODULES")
if env_modules then
    for m in string.gmatch(env_modules, "[^,]+") do
        MODULE_CONFIGS[m] = LOG_LEVELS.DEBUG
    end
end

-- 3. 核心：创建 Logger
function new_logger(module_name)
    -- 获取该模块的日志等级
    local level = MODULE_CONFIGS[module_name] or GLOBAL_LEVEL

    -- 内部日志函数
    -- color: 颜色代码
    -- name:  等级名称 (DEBUG/INFO...)
    -- fmt:   第一个参数 (字符串)
    -- ...:   剩余参数
    local function _log(level_val, name, color, fmt, ...)
        -- 如果等级不够，直接返回
        if level_val < level then
            return
        end

        -- 拼接所有参数为字符串
        local parts = { tostring(fmt) }
        local args = { ... }
        for i = 1, #args do
            parts[#parts+1] = tostring(args[i])
        end
        local str = table.concat(parts, " ")

        -- 组装最终字符串：[模块名] [等级名] 内容
        local msg = ANSI_COLORS.dim .. "[" .. module_name .. "]" .. ANSI_COLORS.reset .. 
                    "[" .. color .. name .. ANSI_COLORS.reset .. "] " .. 
                    str

        -- 输出
        print(msg)
    end

    -- 返回接口
    return {
        debug = function(fmt, ...) _log(LOG_LEVELS.DEBUG, "DEBUG", ANSI_COLORS.cyan, fmt, ...) end,
        info  = function(fmt, ...) _log(LOG_LEVELS.INFO,  "INFO",  ANSI_COLORS.green, fmt, ...) end,
        warn  = function(fmt, ...) _log(LOG_LEVELS.WARN,  "WARN",  ANSI_COLORS.yellow, fmt, ...) end,
        error = function(fmt, ...) _log(LOG_LEVELS.ERROR, "ERROR", ANSI_COLORS.red, fmt, ...) end
    }
end
