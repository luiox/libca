local source_handler = import("libca.em_source_handler")

function get_handler()
    return source_handler.make({
        name = "em_component",
        deps = {"em_base", "em_util"},
        sources = {"ini.c", "scoroutine.c", "skv.c"}
    })
end
