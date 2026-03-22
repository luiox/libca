return function(ctx)
    local _ = ctx
    return {
        name = "hts221",
        dir = "hts221",
        src = {"hts221.c"},
        port_config = {
            mode = {
                default = "extern",
                values = {
                    extern = "LIBCA_HTS221_PORT_MODE=1",
                    dynamic = "LIBCA_HTS221_PORT_MODE=2"
                }
            }
        }
    }
end
