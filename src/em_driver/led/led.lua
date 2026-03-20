return {
    name = "led",
    dir = "led",
    src = {"led.c"},

    port_config = {
        mode = {
            default = "extern",
            values = {
                extern = "LIBCA_LED_PORT_MODE=1",
                dynamic = "LIBCA_LED_PORT_MODE=2"
            }
        },
        extra_cfg = {
            default = "feature_a",
            values = {
                feature_a = "ENABLE_FEATURE_A=1",
                feature_b = "ENABLE_FEATURE_B=1"
            }
        }
    }
}
