-- build_utils.lua

function m_add_libs(target, libname, opts)
    opts = opts or {}

    local libpath = libname:gsub("%.", "/")
    local src_dir = path.join(os.projectdir(), libpath)

    if os.isdir(src_dir) then
        target:add("files", path.join(src_dir, "*.c"))
        target:add("includedirs", src_dir, {public = true})
        print(">>> [Inject OK] " .. libname .. " -> " .. src_dir)
    else
        raise("Library not found: " .. src_dir)
    end

    if opts.port then
        if type(opts.port) == "table" then
            target:add("files", opts.port)
        else
            target:add("files", opts.port)
            local port_dir = path.directory(path.join(os.projectdir(), opts.port))
            target:add("includedirs", port_dir, {public = true})
        end
    end
end
