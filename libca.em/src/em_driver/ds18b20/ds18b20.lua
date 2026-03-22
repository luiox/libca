return function(ctx)
    local _ = ctx
    return {
        name = "ds18b20",
        dir = "ds18b20",
        src = {"ds18b20.c"},
        port_config = {
            mode = {
                default = "extern",
                values = {
                    extern = "LIBCA_DS18B20_PORT_MODE=1",
                    dynamic = "LIBCA_DS18B20_PORT_MODE=2"
                }
            }
        }
    }
end
