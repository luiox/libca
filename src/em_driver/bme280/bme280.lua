return function(ctx)
    local _ = ctx
    return {
        name = "bme280",
        dir = "bme280",
        src = {"bme280.c"},
        port_config = {
            mode = {
                default = "extern",
                values = {
                    extern = "LIBCA_BME280_PORT_MODE=1",
                    dynamic = "LIBCA_BME280_PORT_MODE=2"
                }
            }
        }
    }
end
