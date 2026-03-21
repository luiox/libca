return function(ctx)
    local _ = ctx
    return {
        name = "led",
        dir = "led",
        src = {"led.c"},
        port_config = {
            mode = {
                default = "extern",
                values = {
                    extern = "LIBCA_LED_PORT_MODE=1",
                    dynamic = "LIBCA_LED_PORT_MODE=2"
                }
            }
        }
    }
end
