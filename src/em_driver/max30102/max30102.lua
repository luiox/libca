return function(ctx)
    local _ = ctx
    return {
        name = "max30102",
        dir = "max30102",
        src = {"max30102.c"},
        port_config = {
            mode = {
                default = "extern",
                values = {
                    extern = "LIBCA_MAX30102_PORT_MODE=1",
                    dynamic = "LIBCA_MAX30102_PORT_MODE=2"
                }
            }
        }
    }
end
