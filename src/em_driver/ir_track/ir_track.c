#include "ir_track.h"

#if (LIBCA_IR_TRACK_PORT_MODE == LIBCA_IR_TRACK_PORT_MODE_EXTERN)
static const ir_track_port_t g_ir_track_port_extern_impl = {
    .read_pin = port_ir_track_read_pin,
};
static const ir_track_port_t* g_ir_track_port = &g_ir_track_port_extern_impl;
#elif (LIBCA_IR_TRACK_PORT_MODE == LIBCA_IR_TRACK_PORT_MODE_DYNAMIC)
static const ir_track_port_t* g_ir_track_port = NULL;
#else
#error "Invalid IR_TRACK port mode"
#endif

void ir_track_bind_port(const ir_track_port_t* port)
{
	g_ir_track_port = port;
}

bool ir_track_port_is_registered(void)
{
	return g_ir_track_port != NULL;
}

void ir_track_init(ir_track_t* self, void* gpio, u16 pin)
{
	self->gpio = gpio;
	self->pin  = pin;
}

u8 ir_track_get_value(ir_track_t* self)
{
	return g_ir_track_port->read_pin(self->gpio, self->pin);
}
