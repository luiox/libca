return function(ctx)
    local _ = ctx
    return {
        name = "dht22",
        dir = "dht22",
        src = {"dht22.c"},
        port_config = {
            mode = {
                default = "extern",
                values = {
                    extern = "LIBCA_DHT22_PORT_MODE=1",
                    dynamic = "LIBCA_DHT22_PORT_MODE=2"
                }
            }
        }
    }
end
