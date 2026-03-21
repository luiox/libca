return function(ctx)
    local _ = ctx
    return {
        name = "bh1750",
        dir = "bh1750",
        src = {"bh1750.c"},
        port_config = {
            mode = {
                default = "extern",
                values = {
                    extern = "LIBCA_BH1750_PORT_MODE=1",
                    dynamic = "LIBCA_BH1750_PORT_MODE=2"
                }
            }
        }
    }
end
