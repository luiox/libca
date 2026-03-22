return function(ctx)
    local _ = ctx
    return {
        name = "hc_sr04",
        dir = "hc_sr04",
        src = {"hc_sr04.c"},
        port_config = {
            mode = {
                default = "extern",
                values = {
                    extern = "LIBCA_HC_SR04_PORT_MODE=1",
                    dynamic = "LIBCA_HC_SR04_PORT_MODE=2"
                }
            }
        }
    }
end
