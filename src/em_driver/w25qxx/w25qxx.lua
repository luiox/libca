return function(ctx)
    local _ = ctx
    return {
        name = "w25qxx",
        dir = "w25qxx",
        src = {"w25qxx.c"},
        port_config = {
            mode = {
                default = "extern",
                values = {
                    extern = "LIBCA_W25QXX_PORT_MODE=1",
                    dynamic = "LIBCA_W25QXX_PORT_MODE=2"
                }
            }
        }
    }
end
