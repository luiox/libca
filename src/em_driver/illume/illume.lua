return function(ctx)
    local _ = ctx
    return {
        name = "illume",
        dir = "illume",
        src = {"illume.c"},
        port_config = {
            mode = {
                default = "extern",
                values = {
                    extern = "LIBCA_ILLUME_PORT_MODE=1",
                    dynamic = "LIBCA_ILLUME_PORT_MODE=2"
                }
            }
        }
    }
end
