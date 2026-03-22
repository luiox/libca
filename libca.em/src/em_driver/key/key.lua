return function(ctx)
    local _ = ctx
    return {
        name = "key",
        dir = "key",
        src = {"key.c"},
        port_config = {
            mode = {
                default = "extern",
                values = {
                    extern = "LIBCA_KEY_PORT_MODE=1",
                    dynamic = "LIBCA_KEY_PORT_MODE=2"
                }
            }
        }
    }
end
