return function(ctx)
    local _ = ctx
    return {
        name = "dht11",
        dir = "dht11",
        src = {"dht11.c"},
        port_config = {
            mode = {
                default = "extern",
                values = {
                    extern = "LIBCA_DHT11_PORT_MODE=1",
                    dynamic = "LIBCA_DHT11_PORT_MODE=2"
                }
            }
        }
    }
end
