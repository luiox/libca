return function(ctx)
    local _ = ctx
    return {
        name = "sgp30",
        dir = "sgp30",
        src = {"sgp30.c"},
        port_config = {
            mode = {
                default = "extern",
                values = {
                    extern = "LIBCA_SGP30_PORT_MODE=1",
                    dynamic = "LIBCA_SGP30_PORT_MODE=2"
                }
            }
        }
    }
end
