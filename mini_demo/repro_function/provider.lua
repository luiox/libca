-- 描述域复用函数：不接收 target 参数，直接调用 add_* 接口
function m_add_libs(opts)
    opts = opts or {}

    for _, d in ipairs(opts.defines or {}) do
        add_defines(d)
    end
end
