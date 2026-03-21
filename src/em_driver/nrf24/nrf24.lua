return function(ctx)
    local _ = ctx
    return {
        name = "nrf24",
        dir = "nrf24",
        src = {"nrf24.c"},
        port_config = {
            mode = {
                default = "extern",
                values = {
                    extern = "LIBCA_NRF24_PORT_MODE=1",
                    dynamic = "LIBCA_NRF24_PORT_MODE=2"
                }
            }
        }
    }
end
