return function(ctx)
    local _ = ctx
    return {
        name = "atk_ms53l1m",
        dir = "atk_ms53l1m",
        src = {"atk_ms53l1m.c"},
        port_config = {
            mode = {
                default = "extern",
                values = {
                    extern = "LIBCA_ATK_MS53L1M_PORT_MODE=1",
                    dynamic = "LIBCA_ATK_MS53L1M_PORT_MODE=2"
                }
            }
        }
    }
end
