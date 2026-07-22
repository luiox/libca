return function(ctx)
    local _ = ctx
    return {
        name = "tofxxf",
        dir = "tofxxf",
        deps = {"em_util"},
        src = {"tofxxf.c"},
        port_config = {
            mode = {
                default = "extern",
                values = {
                    extern = "LIBCA_TOFXXF_PORT_MODE=1",
                    dynamic = "LIBCA_TOFXXF_PORT_MODE=2"
                }
            }
        }
    }
end
