local source_handler = import("libca.em_source_handler")

function get_handler()
    return source_handler.make({
        name = "em_bus",
        deps = {"em_base"},
        sources = {"one_wire.c", "soft_i2c.c", "soft_spi.c"}
    })
end
