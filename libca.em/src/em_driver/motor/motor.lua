return function(ctx)
    local _ = ctx
    return {
        name = "motor",
        dir = "motor",
        src = {"motor.c"},
        port_config = {
            mode = {
                default = "extern",
                values = {
                    extern = "LIBCA_MOTOR_PORT_MODE=1",
                    dynamic = "LIBCA_MOTOR_PORT_MODE=2"
                }
            }
        }
    }
end
