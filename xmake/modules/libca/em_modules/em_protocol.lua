-- em_protocol module handler

local source_handler = import("libca.em_source_handler")

function get_handler()
    return source_handler.make({
        name = "em_protocol",
        deps = {"em_base", "em_util"},
        sources = {"dummy.c", "file_transfer.c", "xmodem.c", "ymodem.c"}
    })
end
