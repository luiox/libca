-- em_util module handler

local source_handler = import("libca.em_source_handler")

function get_handler()
    return source_handler.make({
        name = "em_util",
        deps = {"em_base"},
        sources = {
            "bitmap.c",
            "bits_util.c",
            "crc.c",
            "doubly_linked_list.c",
            "doubly_list.c",
            "endian_util.c",
            "filter.c",
            "lifo.c",
            "math_util.c",
            "mem_view.c",
            "memory_pool.c",
            "pid.c",
            "queue.c",
            "singly_list.c",
            "soft_timer.c",
            "stack.c"
        }
    })
end
