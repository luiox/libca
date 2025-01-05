#ifndef LIBCA_EVENT_EVENT_H
#define LIBCA_EVENT_EVENT_H

typedef struct
{
    int   type;
    void* data;
} event_t;

typedef void (*event_handler_t)(event_t* event);

void register_event_handler(int type, event_handler_t handler);

void send_event(event_t* event);

void event_loop();


#endif   // !LIBCA_EVENT_EVENT_H
