return function(ctx)
    local _ = ctx
    return {
        name = "mq_x",
        dir = "mq_x",
        src = {"mq_x.c"},
        port_config = {
            mode = {
                default = "extern",
                values = {
                    extern = "LIBCA_MQ_X_PORT_MODE=1",
                    dynamic = "LIBCA_MQ_X_PORT_MODE=2"
                }
            }
        }
    }
end
