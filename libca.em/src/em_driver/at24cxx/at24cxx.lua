return function(ctx)
    local _ = ctx
    return {
        name = "at24cxx",
        dir = "at24cxx",
        src = {"at24cxx.c"},
        port_config = {
            mode = {
                default = "extern",
                values = {
                    extern = "LIBCA_AT24CXX_PORT_MODE=1",
                    dynamic = "LIBCA_AT24CXX_PORT_MODE=2"
                }
            }
        }
    }
end
