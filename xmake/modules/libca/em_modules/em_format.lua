local source_handler = import("libca.em_source_handler")

function get_handler()
    return source_handler.make({
        name = "em_format",
        deps = {"em_base"},
        sources = {"format.c"}
    })
end
