-- led driver spec

local spec = import("libca.em_spec")

function get_spec()
    return spec.make_port_mode_driver({
        driver_name = "led",
        rel_dir = "em_driver/led",
        source = "led.c",
        default_port_source = "port_led.c",
        define_extern = "LIBCA_LED_PORT_MODE=1",
        define_dynamic = "LIBCA_LED_PORT_MODE=2"
    })
end