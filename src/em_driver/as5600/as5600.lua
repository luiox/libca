return function(ctx)
    local _ = ctx
    return {
        name = "as5600",
        dir = "as5600",
        src = {"as5600.c"},
        port_config = {
            mode = {
                default = "extern",
                values = {
                    extern = "LIBCA_AS5600_PORT_MODE=1",
                    dynamic = "LIBCA_AS5600_PORT_MODE=2"
                }
            }
        }
    }
end
