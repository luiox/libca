---
name: em-driver-dev
description: This skill should be used when developing em_driver hardware drivers for embedded MCU systems. Use it for creating new sensor/peripheral drivers, implementing port layer abstractions, and following the OOP-style driver pattern defined in the libca project. Triggered by requests like "develop a xxx driver" or "implement a sensor driver for [device name]".
---

# EM Driver Development Guide

## Overview

This skill guides the development of em_driver hardware drivers for embedded MCU systems. It enforces the standardized OOP-style driver pattern with port layer abstraction, ensuring consistent code structure and hardware independence across the libca project.

## Driver Development Workflow

### Step 1: Understand Hardware Requirements

Analyze the target hardware device:

1. **Identify the communication interface**:
   - GPIO (direct pin control)
   - I2C bus
   - SPI bus
   - UART/Serial
   - Custom timing sequences (bit-banging)

2. **Determine required port functions**:
   - For GPIO: `write_pin`, `read_pin`, `set_mode` (output/input)
   - For I2C: `i2c_write`, `i2c_read`
   - For timing-sensitive: `delay_us`, `delay_ms`

3. **Identify device configuration parameters**:
   - Hardware pins/addresses
   - Configuration modes
   - Calibration data

### Step 2: Define Driver Structure

Follow the standard em_driver pattern in this order:

#### 2.1 Port Layer (Hardware Abstraction)

Define `xxx_port_t` with function pointers for hardware operations:

```c
typedef struct xxx_port {
    void (*write_pin)(void* gpio, u16 pin, u8 value);
    u8 (*read_pin)(void* gpio, u16 pin);
    void (*delay_us)(u32 us);
} xxx_port_t;
```

**Rules**:
- Use `void*` for handle types to avoid platform-specific dependencies
- Include only the minimum necessary functions
- Group related functions logically

#### 2.2 Port Binding Functions

Always include these two functions if port layer exists:

```c
void xxx_bind_port(const xxx_port_t* port);
bool xxx_port_is_registered(void);
```

**Implementation pattern**:

```c
static const xxx_port_t* g_xxx_port = NULL;

void xxx_bind_port(const xxx_port_t* port) {
    g_xxx_port = port;
}

bool xxx_port_is_registered(void) {
    return g_xxx_port != NULL;
}
```

#### 2.3 Device Object (`xxx_t`)

Define the device structure with hardware resources and state:

```c
typedef struct xxx {
    void* gpio;       // GPIO handle
    u16 pin;          // Pin number
    void* hi2c;       // I2C handle (if needed)
    // Add state variables and configuration
    u8 initialized;
    // ... other members
} xxx_t;
```

**Rules**:
- Hardware handles first (gpio, hi2c, etc.)
- Configuration parameters next
- State variables last

#### 2.4 API Functions

Follow these conventions:

1. **All functions must have `xxx_t* self` as the first parameter** (OOP style)
2. **Always include `xxx_init()`** - even if it does nothing:

```c
void xxx_init(xxx_t* self, /* configuration parameters */) {
    // nothing to do - if truly empty
}
```

3. **Define error codes** (if applicable):

```c
#define XXX_OK                         0
#define XXX_ERR_PORT_NOT_REGISTERED   (-1)
#define XXX_ERR_INVALID_PARAM         (-2)
// ... all error codes must be negative
```

4. **Use access macros** to simplify code:

```c
#define XXX_WRITE(self, v)    g_xxx_port->write_pin((self)->gpio, (self)->pin, (v))
#define XXX_READ(self)        g_xxx_port->read_pin((self)->gpio, (self)->pin)
#define XXX_DELAY_US(us)      g_xxx_port->delay_us(us)
```

### Step 3: Implement Driver Logic

#### 3.1 Include Debug System

```c
#include "../em_base/debug.h"

// Use debug_print for logging
debug_print("[xxx] error: port not registered\n");

// Use param_check for parameter validation
param_check(self != NULL);
param_check(g_xxx_port != NULL);
```

#### 3.2 Check Port Registration

Always verify port is registered before use:

```c
i32 xxx_read(xxx_t* self, u8* data) {
    if (!g_xxx_port) {
        debug_print("[xxx] error: port not registered\n");
        return XXX_ERR_PORT_NOT_REGISTERED;
    }
    // ... rest of implementation
}
```

#### 3.3 Implement Device Operations

Use the port functions through access macros:

```c
i32 xxx_read_data(xxx_t* self, u8* buffer) {
    param_check(self != NULL);
    param_check(buffer != NULL);
    param_check(g_xxx_port != NULL);

    XXX_WRITE(self, 0);  // Example: write to trigger read
    XXX_DELAY_US(10);   // Wait for device
    *buffer = XXX_READ(self);

    return XXX_OK;
}
```

### Step 4: Add Driver to Build System

Edit `src/em_driver/xmake.lua` to include the new driver:

```lua
-- Add the driver to the list
target("libca.em_driver.xxx")
    add_files("xxx.c")
```

### Step 5: Testing Approach

**IMPORTANT**: em_driver modules do NOT require unit tests because they depend on hardware.

Instead, test by:
1. **MCU environment**: Bind real HAL functions and test on hardware
2. **Simulation environment**: Bind simulated functions to verify logic

**Example simulation test**:

```c
// Simulated port functions
u8 simulated_value = 0;
void sim_write_pin(void* gpio, u16 pin, u8 value) {
    simulated_value = value;
    printf("[SIM] GPIO %c%d = %d\n", (char)gpio, pin, value);
}
u8 sim_read_pin(void* gpio, u16 pin) {
    return simulated_value;
}

// Bind simulated port
xxx_port_t sim_port = {
    .write_pin = sim_write_pin,
    .read_pin = sim_read_pin,
    .delay_us = NULL  // Not needed for this driver
};
xxx_bind_port(&sim_port);
```

## Common Driver Patterns

### Simple GPIO Driver (LED)

**Port**:
```c
typedef struct led_port {
    void (*write_pin)(void* gpio, u16 pin, u8 value);
} led_port_t;
```

**Device**:
```c
typedef struct led {
    void* gpio;
    u16 pin;
    u8 active_level;  // 0 or 1
} led_t;
```

**API**:
- `led_init(led_t* self, void* gpio, u16 pin, u8 active_level)`
- `led_on(led_t* self)`
- `led_off(led_t* self)`
- `led_toggle(led_t* self)`

### I2C Sensor Driver (BH1750)

**Port**:
```c
typedef struct bh1750_port {
    i32 (*i2c_write)(void* hi2c, u16 dev_addr, u16 mem_addr,
                     u16 mem_addr_size, u8* data, u16 data_size, u32 timeout);
    i32 (*i2c_read)(void* hi2c, u16 dev_addr, u16 mem_addr,
                    u16 mem_addr_size, u8* data, u16 data_size, u32 timeout);
} bh1750_port_t;
```

**Device**:
```c
typedef struct bh1750 {
    void* hi2c;
    u16 dev_addr;
} bh1750_t;
```

### Timing-Sensitive Sensor (DHT11)

**Port**:
```c
typedef struct dht11_port {
    void (*write_pin)(void* gpio, u16 pin, u8 value);
    u8 (*read_pin)(void* gpio, u16 pin);
    void (*set_output_mode)(void* gpio, u16 pin);
    void (*set_input_mode)(void* gpio, u16 pin);
    void (*delay_us)(u32 us);
    void (*delay_ms)(u32 ms);
} dht11_port_t;
```

**Device**:
```c
typedef struct dht11 {
    void* gpio;
    u16 pin;
} dht11_t;
```

## Naming Conventions

- **Type names**: `xxx_t` (device), `xxx_port_t` (port)
- **Functions**: `xxx_bind_port()`, `xxx_init()`, `xxx_function()`
- **Macros**: `XXX_WRITE()`, `XXX_READ()`, `XXX_OK`, `XXX_ERR_*`
- **Error codes**: Must be negative numbers, e.g., `XXX_ERR_NOT_INITIALIZED = (-2)`

## When NOT to Use OOP Style

For utility functions that don't require a device object:
- Do NOT create `xxx_t` structure
- Do NOT pass `xxx_t* self` parameter
- These are standalone helper functions

**Example**: A function to calculate CRC for a device doesn't need OOP:

```c
// NO OOP needed here
u32 xxx_calculate_crc(const u8* data, u32 length) {
    // Implementation
    return crc_value;
}
```

## Resources

### scripts/

**generate_driver_skeleton.py**: Generate driver code skeleton

Run this script to automatically create the basic structure for a new driver:

```bash
python scripts/generate_driver_skeleton.py <driver_name> <interface_type>
```

Supported interface types: `gpio`, `i2c`, `spi`, `uart`, `timing`

The script will generate:
- `xxx.h` with port definition, device object, and API declarations
- `xxx.c` with port binding, init function, and stub implementations
- Properly formatted with libca coding standards

### references/

**em_driver±‡–¥πÊ∑∂.md**: Complete Chinese specification document

Refer to this document for:
- Detailed coding standards
- Code style requirements
- Best practices and examples
- Common pitfalls to avoid

**driver_examples.md**: Analysis of existing drivers

This document contains:
- LED driver (simple GPIO)
- BH1750 driver (I2C sensor)
- DHT11 driver (timing-sensitive)
- EC11 driver (state machine)
- AT24CXX driver (EEPROM with I2C)

Each example includes:
- Port design rationale
- Device object structure
- Key implementation details
- Usage examples

---

**Note**: This skill does NOT require unit tests. Drivers are tested in real hardware or simulation environments.
