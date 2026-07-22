local source_handler = import("libca.em_source_handler")

function get_handler()
    return source_handler.make({
        name = "em_mpool",
        deps = {"em_base", "em_util"},
        sources = {"fixed_allocator.c"}
    })
end
