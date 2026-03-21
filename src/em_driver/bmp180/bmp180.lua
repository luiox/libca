return function(ctx)
    local _ = ctx
    return {
        name = "bmp180",
        dir = "bmp180",
        src = {"bmp180.c"},
        port_config = {
            mode = {
                default = "extern",
                values = {
                    extern = "LIBCA_BMP180_PORT_MODE=1",
                    dynamic = "LIBCA_BMP180_PORT_MODE=2"
                }
            }
        }
    }
end
