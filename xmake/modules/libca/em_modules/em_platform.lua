local source_handler = import("libca.em_source_handler")

function get_handler()
    return source_handler.make({
        name = "em_platform",
        deps = {"em_base"},
        sources = {"async.c", "time_util.c"}
    })
end
