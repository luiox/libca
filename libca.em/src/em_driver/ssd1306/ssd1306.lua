return function(ctx)
    local _ = ctx
    return {
        name = "ssd1306",
        dir = "ssd1306",
        src = {"ssd1306.c", "ssd1306_font.c"},
        port_config = {
            mode = {
                default = "extern",
                values = {
                    extern = "LIBCA_SSD1306_PORT_MODE=1",
                    dynamic = "LIBCA_SSD1306_PORT_MODE=2"
                }
            }
        }
    }
end
