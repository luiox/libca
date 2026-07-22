local source_handler = import("libca.em_source_handler")

function get_handler()
    return source_handler.make({
        name = "em_dstream",
        deps = {"em_base"},
        sources = {
            "delimiter_parser.c",
            "ds_fixed_buffer.c",
            "ds_ring_buffer.c",
            "fixed_buffer.c",
            "length_parser.c",
            "pingpong_buffer.c",
            "ring_buffer.c"
        }
    })
end
