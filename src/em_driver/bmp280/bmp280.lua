return function(ctx)
    local _ = ctx
    return {
        name = "bmp280",
        dir = "bmp280",
        src = {"bmp280.c"},
        port_config = {
            mode = {
                default = "extern",
                values = {
                    extern = "LIBCA_BMP280_PORT_MODE=1",
                    dynamic = "LIBCA_BMP280_PORT_MODE=2"
                }
            }
        }
    }
end
