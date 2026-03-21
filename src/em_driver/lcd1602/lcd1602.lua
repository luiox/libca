return function(ctx)
    local _ = ctx
    return {
        name = "lcd1602",
        dir = "lcd1602",
        src = {"lcd1602.c"},
        port_config = {
            mode = {
                default = "extern",
                values = {
                    extern = "LIBCA_LCD1602_PORT_MODE=1",
                    dynamic = "LIBCA_LCD1602_PORT_MODE=2"
                }
            }
        }
    }
end
