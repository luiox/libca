local source_handler = import("libca.em_source_handler")

function get_handler()
    return source_handler.make({
        name = "em_crypto",
        deps = {"em_base"},
        sources = {"base64.c", "crypto.c"}
    })
end
