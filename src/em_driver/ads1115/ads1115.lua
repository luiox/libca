return function(ctx)
    local _ = ctx
    return {
        name = "ads1115",
        dir = "ads1115",
        src = {"ads1115.c"},
        port_config = {
            mode = {
                default = "extern",
                values = {
                    extern = "LIBCA_ADS1115_PORT_MODE=1",
                    dynamic = "LIBCA_ADS1115_PORT_MODE=2"
                }
            }
        }
    }
end
