function my_add_libs(name)
    add_defines("LIB_" .. name)
    add_files("src/" .. name .. ".c")
end
