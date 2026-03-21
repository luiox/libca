return function(ctx)
    local _ = ctx
    return {
        name = "ec11",
        dir = "ec11",
        src = {"ec11.c"},
        port_config = {
            mode = {
                default = "extern",
                values = {
                    extern = "LIBCA_EC11_PORT_MODE=1",
                    dynamic = "LIBCA_EC11_PORT_MODE=2"
                }
            }
        }
    }
end
