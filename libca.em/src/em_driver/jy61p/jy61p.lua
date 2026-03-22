return function(ctx)
    local _ = ctx
    return {
        name = "jy61p",
        dir = "jy61p",
        src = {"jy61p.c"},
        port_config = {
            mode = {
                default = "extern",
                values = {
                    extern = "LIBCA_JY61P_PORT_MODE=1",
                    dynamic = "LIBCA_JY61P_PORT_MODE=2"
                }
            }
        }
    }
end
