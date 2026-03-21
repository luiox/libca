return function(ctx)
    local _ = ctx
    return {
        name = "ds1302",
        dir = "ds1302",
        src = {"ds1302.c"},
        port_config = {
            mode = {
                default = "extern",
                values = {
                    extern = "LIBCA_DS1302_PORT_MODE=1",
                    dynamic = "LIBCA_DS1302_PORT_MODE=2"
                }
            }
        }
    }
end
