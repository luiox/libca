return function(ctx)
    local _ = ctx
    return {
        name = "ir_track",
        dir = "ir_track",
        src = {"ir_track.c"},
        port_config = {
            mode = {
                default = "extern",
                values = {
                    extern = "LIBCA_IR_TRACK_PORT_MODE=1",
                    dynamic = "LIBCA_IR_TRACK_PORT_MODE=2"
                }
            }
        }
    }
end
