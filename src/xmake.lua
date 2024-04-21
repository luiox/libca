target("ca")
    -- set_kind("binary")
    set_kind("$(kind)")
    add_includedirs("../include")
    add_files("**.cpp")
    -- set_basename("ca")
target_end()

