function add_libs(target, name)
    target:add("defines", "LIB_" .. name)
    target:add("files", "src/" .. name .. ".c")
end
