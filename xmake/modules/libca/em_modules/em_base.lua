-- em_base module spec

local spec = import("libca.em_spec")

function get_spec()
    return spec.make_simple_module({
        files = {
            "em_base/datatype.c",
            "em_base/debug.c",
            "em_base/compiler_compat.c"
        }
    })
end