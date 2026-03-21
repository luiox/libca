return function(ctx)
    local _ = ctx
    return {
        name = "bmc050",
        dir = "bmc050",
        src = {"bmc050.c"},
        port_config = {
            mode = {
                default = "extern",
                values = {
                    extern = "LIBCA_BMC050_PORT_MODE=1",
                    dynamic = "LIBCA_BMC050_PORT_MODE=2"
                }
            }
        }
    }
end
