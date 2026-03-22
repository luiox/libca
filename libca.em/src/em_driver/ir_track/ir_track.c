#include "ir_track.h"

////////////////////////////////////////////////////////////////////////////////

#if (LIBCA_IR_TRACK_PORT_MODE == LIBCA_IR_TRACK_PORT_MODE_EXTERN)
#define IR_TRACK_READ_PIN(gpio, pin) port_ir_track_read_pin((gpio), (pin))

#elif (LIBCA_IR_TRACK_PORT_MODE == LIBCA_IR_TRACK_PORT_MODE_DYNAMIC)
static const ir_track_port_t* g_ir_track_port = NULL;
#define IR_TRACK_READ_PIN(gpio, pin) g_ir_track_port->read_pin((gpio), (pin))

#else
#error "Invalid IR_TRACK port mode"
#endif

#if (LIBCA_IR_TRACK_PORT_MODE == LIBCA_IR_TRACK_PORT_MODE_DYNAMIC)
void ir_track_bind_port(const ir_track_port_t* port) { g_ir_track_port = port; }
bool ir_track_port_is_registered(void) { return g_ir_track_port != NULL; }
#endif

////////////////////////////////////////////////////////////////////////////////

void ir_track_init(ir_track_t* self, void* gpio, u16 pin)
{
	self->gpio = gpio;
	self->pin  = pin;
}

u8 ir_track_get_value(ir_track_t* self)
{
	return IR_TRACK_READ_PIN(self->gpio, self->pin);
}
