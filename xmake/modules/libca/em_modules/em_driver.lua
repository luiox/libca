-- em_driver aggregate module spec

local spec = import("libca.em_spec")

function get_spec()
    return spec.make_driver_module({
        deps = {"em_base"}
    })
end