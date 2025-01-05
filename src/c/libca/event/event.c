#include <libca/event/event.h>

void register_event_handler(int type, event_handler_t handler)
{
    // TODO
}

void send_event(event_t* event) {}

void event_loop()
{
    while (1) {
        // event_t *event = get_event();
        // send_event(event);
        // free(event);
    }
}
