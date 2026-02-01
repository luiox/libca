# Driver Development Workflow

This document provides the complete workflow for developing new em_driver hardware drivers and understanding the driver framework design for maintenance purposes. It serves as the primary guide for both new driver development and existing driver analysis.

## Table of Contents

1. [Workflow Overview](#workflow-overview)
2. [Step 1: Understand Hardware Requirements](#step-1-understand-hardware-requirements)
3. [Step 2: Define Driver Structure](#step-2-define-driver-structure)
4. [Step 3: Implement Driver Logic](#step-3-implement-driver-logic)
5. [Step 4: Integrate into Build System](#step-4-integrate-into-build-system)
6. [Step 5: Testing Approach](#step-5-testing-approach)
7. [Maintenance Workflow](#maintenance-workflow)

---

## Workflow Overview

```
┌─────────────────────────────────────────────────────────────┐
│                    Driver Development Cycle                 │
├─────────────────────────────────────────────────────────────┤
│                                                              │
│   1. Analyze Hardware        ───┐                           │
│      └─ Communication         │                           │
│      └─ Port functions        │                           │
│      └─ Configuration         │                           │
│                               │                           │
│   2. Define Structure        <──┤                           │
│      └─ Port layer            │                           │
│      └─ Device object         │                           │
│      └─ API functions         │                           │
│                               │                           │
│   3. Implement Logic        <──┤                           │
│      └─ Debug system          │                           │
│      └─ Port checks           │                           │
│      └─ Device operations     │                           │
│                               │                           │
│   4. Integrate Build        <──┤                           │
│      └─ xmake.lua             │                           │
│                               │                           │
│   5. Test                  <──┘                           │
│      └─ MCU environment                                   │
│      └─ Simulation                                       │
│                                                              │
└─────────────────────────────────────────────────────────────┘
```

---

## Step 1: Understand Hardware Requirements

### 1.1 Identify Communication Interface

Analyze how the device communicates with the MCU:

| Interface Type | Characteristics | Example Devices |
|----------------|----------------|-----------------|
| **GPIO** | Direct pin control, simple on/off | LEDs, buzzers, relays |
| **I2C** | Two-wire serial, addressing | Sensors, EEPROMs, RTCs |
| **SPI** | Four-wire high-speed serial | Flash, displays, RF modules |
| **UART** | Asynchronous serial | GPS modules, Bluetooth |
| **Custom Timing** | Bit-banging with precise timing | DHT11, one-wire devices |

### 1.2 Determine Required Port Functions

For each interface type, identify the minimum hardware operations needed:

**GPIO Interface**:
```c
void (*write_pin)(void* gpio, u16 pin, u8 value);
u8  (*read_pin)(void* gpio, u16 pin);
void (*set_mode)(void* gpio, u16 pin, u8 mode);  // output/input
```

**I2C Interface**:
```c
i32 (*i2c_write)(void* hi2c, u16 dev_addr, u16 mem_addr,
                 u16 mem_addr_size, u8* data, u16 data_size, u32 timeout);
i32 (*i2c_read)(void* hi2c, u16 dev_addr, u16 mem_addr,
                u16 mem_addr_size, u8* data, u16 data_size, u32 timeout);
```

**Timing-Sensitive Interface**:
```c
void (*delay_us)(u32 us);
void (*delay_ms)(u32 ms);
```

### 1.3 Identify Device Configuration Parameters

Extract all configurable aspects from the datasheet:

- **Hardware resources**: GPIO pins, I2C addresses, SPI chip select pins
- **Operating modes**: Measurement range, resolution, power mode
- **Calibration data**: Offset, scale factors, sensitivity
- **Timing constraints**: Minimum delays, timeout values

**Example - BH1750 Light Sensor**:
```c
// Hardware
void* hi2c;
u16 dev_addr;  // 0x46 or 0x47 depending on ADDR pin

// Configuration
u8 measurement_mode;  // Continuous/One-time, resolution
u8 mt_reg;           // Measurement time register
```

---

## Step 2: Define Driver Structure

### 2.1 Port Layer (Hardware Abstraction)

Define `xxx_port_t` with function pointers for hardware operations.

**Design Rules**:
- Use `void*` for handle types to avoid platform dependencies
- Include only the minimum necessary functions
- Group related functions logically
- Keep the port structure small and focused

**Example - Simple GPIO Driver**:
```c
typedef struct led_port {
    void (*write_pin)(void* gpio, u16 pin, u8 value);
} led_port_t;
```

**Example - I2C Sensor Driver**:
```c
typedef struct bh1750_port {
    i32 (*i2c_write)(void* hi2c, u16 dev_addr, u16 mem_addr,
                     u16 mem_addr_size, u8* data, u16 data_size, u32 timeout);
    i32 (*i2c_read)(void* hi2c, u16 dev_addr, u16 mem_addr,
                    u16 mem_addr_size, u8* data, u16 data_size, u32 timeout);
} bh1750_port_t;
```

**Example - Timing-Sensitive Driver**:
```c
typedef struct dht11_port {
    void (*write_pin)(void* gpio, u16 pin, u8 value);
    u8  (*read_pin)(void* gpio, u16 pin);
    void (*set_output_mode)(void* gpio, u16 pin);
    void (*set_input_mode)(void* gpio, u16 pin);
    void (*delay_us)(u32 us);
    void (*delay_ms)(u32 ms);
} dht11_port_t;
```

### 2.2 Port Binding Functions

Always include these two functions if a port layer exists:

```c
void xxx_bind_port(const xxx_port_t* port);
bool xxx_port_is_registered(void);
```

**Implementation Pattern**:
```c
// Global port pointer
static const xxx_port_t* g_xxx_port = NULL;

// Register port
void xxx_bind_port(const xxx_port_t* port) {
    g_xxx_port = port;
}

// Check if port is registered
bool xxx_port_is_registered(void) {
    return g_xxx_port != NULL;
}
```

**Why this pattern?**:
- Single global port is sufficient for most drivers (shared hardware interface)
- Allows easy testing with simulated ports
- Prevents NULL dereferences with `port_is_registered()`

### 2.3 Device Object (`xxx_t`)

Define the device structure with hardware resources and state.

**Member Organization**:
```c
typedef struct xxx {
    // 1. Hardware handles (always first)
    void* gpio;       // GPIO handle
    u16 pin;          // Pin number
    void* hi2c;       // I2C handle (if needed)

    // 2. Configuration parameters
    u8 mode;          // Operating mode
    u16 timeout;      // Timeout in ms

    // 3. State variables (always last)
    u8 initialized;
    u8 last_error;
} xxx_t;
```

**Example - LED Driver**:
```c
typedef struct led {
    void* gpio;       // Hardware: GPIO handle
    u16 pin;          // Hardware: Pin number
    u8 active_level;  // Config: Active level (0 or 1)
} led_t;
```

**Example - DHT11 Sensor**:
```c
typedef struct dht11 {
    void* gpio;       // Hardware: GPIO handle
    u16 pin;          // Hardware: Pin number
    u8 last_valid;    // State: Last read was valid
} dht11_t;
```

### 2.4 API Functions

Follow these naming and design conventions:

#### 2.4.1 Function Signatures

**All functions must have `xxx_t* self` as the first parameter** (OOP style):

```c
void xxx_init(xxx_t* self, /* config params */);
i32  xxx_read(xxx_t* self, u8* data);
void xxx_reset(xxx_t* self);
```

#### 2.4.2 Init Function

**Always include `xxx_init()`** - even if it does nothing:

```c
void xxx_init(xxx_t* self, void* gpio, u16 pin) {
    // Nothing to initialize if all state is in self
    // But the function must exist for consistency
}
```

**If initialization is needed**:
```c
void xxx_init(xxx_t* self, void* gpio, u16 pin, u8 mode) {
    self->gpio = gpio;
    self->pin = pin;
    self->mode = mode;
    self->initialized = 1;
}
```

#### 2.4.3 Error Codes

Define error codes (if applicable):

```c
#define XXX_OK                         0
#define XXX_ERR_PORT_NOT_REGISTERED   (-1)
#define XXX_ERR_INVALID_PARAM         (-2)
#define XXX_ERR_NOT_INITIALIZED       (-3)
#define XXX_ERR_TIMEOUT               (-4)

// All error codes must be negative!
```

#### 2.4.4 Access Macros

Define access macros to simplify code:

```c
// For GPIO-based drivers
#define XXX_WRITE(self, v)    g_xxx_port->write_pin((self)->gpio, (self)->pin, (v))
#define XXX_READ(self)        g_xxx_port->read_pin((self)->gpio, (self)->pin)

// For timing
#define XXX_DELAY_US(us)      g_xxx_port->delay_us(us)

// For I2C
#define XXX_I2C_READ(reg, buf, len) \
    g_xxx_port->i2c_read((self)->hi2c, (self)->dev_addr, (reg), \
                         1, (buf), (len), 1000)
```

**Benefits**:
- Cleaner implementation code
- Easy to modify port call if needed
- Self-documenting code

---

## Step 3: Implement Driver Logic

### 3.1 Include Debug System

```c
#include "../em_base/debug.h"

// Use debug_print for logging
debug_print("[xxx] error: port not registered\n");

// Use param_check for parameter validation
param_check(self != NULL);
param_check(g_xxx_port != NULL);
```

### 3.2 Check Port Registration

Always verify port is registered before use:

```c
i32 xxx_read(xxx_t* self, u8* data) {
    // Check port first
    if (!g_xxx_port) {
        debug_print("[xxx] error: port not registered\n");
        return XXX_ERR_PORT_NOT_REGISTERED;
    }

    // Then check parameters
    param_check(self != NULL);
    param_check(data != NULL);

    // Implementation
    // ...
}
```

### 3.3 Implement Device Operations

Use port functions through access macros:

```c
i32 xxx_read_data(xxx_t* self, u8* buffer) {
    param_check(self != NULL);
    param_check(buffer != NULL);
    param_check(g_xxx_port != NULL);

    // Write command to device
    XXX_WRITE(self, 1);   // Trigger read

    // Wait for device
    XXX_DELAY_US(10);

    // Read data
    *buffer = XXX_READ(self);

    return XXX_OK;
}
```

### 3.4 Error Handling Pattern

```c
i32 xxx_operation(xxx_t* self, u8* data) {
    // 1. Check port
    if (!g_xxx_port) {
        debug_print("[xxx] error: port not registered\n");
        return XXX_ERR_PORT_NOT_REGISTERED;
    }

    // 2. Validate parameters
    param_check(self != NULL);
    param_check(data != NULL);

    // 3. Check initialization state (if applicable)
    if (!self->initialized) {
        debug_print("[xxx] error: not initialized\n");
        return XXX_ERR_NOT_INITIALIZED;
    }

    // 4. Implement operation
    // ... driver-specific logic

    return XXX_OK;
}
```

---

## Step 4: Integrate into Build System

### 4.1 Add Driver to xmake.lua

Edit `src/em_driver/xmake.lua` to include the new driver:

```lua
-- Add the driver target
target("libca.em_driver.xxx")
    add_files("xxx.c")
    add_headerfiles("xxx.h", {prefixdir = "libca/em_driver"})

-- Ensure it's included in the main em_driver target
-- (if using a combined library)
```

### 4.2 Build Verification

```bash
# Build the driver
xmake build libca.em_driver.xxx

# Build entire em_driver module
xmake build libca.em_driver
```

### 4.3 Header File Organization

Ensure `xxx.h` is properly formatted:

```c
#ifndef LIBCA_EM_DRIVER_XXX_H
#define LIBCA_EM_DRIVER_XXX_H

#include "em_base/datatype.h"

// Forward declarations
typedef struct xxx_port xxx_port_t;
typedef struct xxx xxx_t;

// Port definition
struct xxx_port {
    // Function pointers...
};

// Device definition
struct xxx {
    // Members...
};

// Port binding
void xxx_bind_port(const xxx_port_t* port);
bool xxx_port_is_registered(void);

// API functions
void xxx_init(xxx_t* self, /* params */);
i32  xxx_read(xxx_t* self, u8* data);

// Error codes
#define XXX_OK       0
// ...

#endif // LIBCA_EM_DRIVER_XXX_H
```

---

## Step 5: Testing Approach

### 5.1 Important: No Unit Tests for Drivers

**em_driver modules do NOT require unit tests** because they depend on hardware.

Instead, test by:
1. **MCU environment**: Bind real HAL functions and test on hardware
2. **Simulation environment**: Bind simulated functions to verify logic

### 5.2 Simulation Testing

Create a simulation environment to test driver logic without hardware:

```c
// Simulated port functions
static u8 simulated_pin_state = 0;

void sim_write_pin(void* gpio, u16 pin, u8 value) {
    simulated_pin_state = value;
    printf("[SIM] GPIO %c%d = %d\n", (char)gpio, pin, value);
}

u8 sim_read_pin(void* gpio, u16 pin) {
    printf("[SIM] GPIO %c%d read -> %d\n", (char)gpio, pin, simulated_pin_state);
    return simulated_pin_state;
}

void sim_delay_us(u32 us) {
    printf("[SIM] delay_us(%d)\n", us);
}

// Bind simulated port
xxx_port_t sim_port = {
    .write_pin = sim_write_pin,
    .read_pin = sim_read_pin,
    .delay_us = sim_delay_us
};

// Test the driver
xxx_t my_device;
xxx_init(&my_device, (void*)"A", 5);
xxx_bind_port(&sim_port);

// Test operations
xxx_write(&my_device, 1);
u8 value = xxx_read(&my_device);
```

### 5.3 Hardware Testing

On the actual MCU:

```c
// Bind real HAL functions
xxx_port_t hal_port = {
    .write_pin = HAL_GPIO_WritePin,
    .read_pin = HAL_GPIO_ReadPin,
    .delay_us = HAL_Delay_us
};
xxx_bind_port(&hal_port);

// Test with real device
xxx_t sensor;
xxx_init(&sensor, GPIOA, GPIO_PIN_5);
xxx_read(&sensor, &data);
```

### 5.4 Test Checklist

- [ ] Port registration check
- [ ] Parameter validation with `param_check`
- [ ] All API functions work correctly
- [ ] Error codes are returned appropriately
- [ ] Device operates within datasheet specifications
- [ ] Multiple instances work (if supported)

---

## Maintenance Workflow

When maintaining or analyzing existing drivers:

### 1. Understand the Design

Use this workflow to understand any em_driver:

1. **Check port structure** (`xxx_port_t`):
   - What hardware operations are needed?
   - What communication interface does it use?

2. **Examine device object** (`xxx_t`):
   - What hardware resources does it need?
   - What configuration options exist?
   - What state does it maintain?

3. **Review API functions**:
   - What operations are supported?
   - How is error handling done?
   - Are there any access macros?

4. **Study implementation**:
   - How does it use the port layer?
   - What are the timing requirements?
   - Are there any state machines?

### 2. Common Maintenance Tasks

#### Adding a New Feature

Follow Steps 1-4 of this workflow:
1. Analyze new hardware requirements
2. Update device structure and API
3. Implement new logic
4. Test with simulation then hardware

#### Debugging an Issue

1. **Check port registration**: Is `xxx_port_is_registered()` returning true?
2. **Verify parameters**: Are all `param_check` calls passing?
3. **Review timing**: Are delays sufficient for the hardware?
4. **Examine state**: Check internal state variables

#### Porting to New Platform

1. Implement the port functions using the new HAL
2. Bind the new port with `xxx_bind_port()`
3. Test on the new hardware

---

## Quick Reference

### Common File Structure

```
xxx.h:
  - Forward declarations
  - Port structure (xxx_port_t)
  - Device structure (xxx_t)
  - Port binding functions
  - API declarations
  - Error codes

xxx.c:
  - Global port pointer
  - Port binding implementation
  - Access macros
  - API implementation
```

### Essential Patterns

```c
// Port pattern
static const xxx_port_t* g_xxx_port = NULL;

void xxx_bind_port(const xxx_port_t* port) {
    g_xxx_port = port;
}

// Check pattern
if (!g_xxx_port) {
    debug_print("[xxx] error: port not registered\n");
    return XXX_ERR_PORT_NOT_REGISTERED;
}

// Validate pattern
param_check(self != NULL);
param_check(data != NULL);

// Access pattern
XXX_WRITE(self, 1);
value = XXX_READ(self);
```

### Related Documents

- [Common Driver Patterns](./02_Common_Driver_Patterns.md) - Detailed examples
- [Design Principles](./03_Design_Principles.md) - Best practices and guidelines
- [Driver Examples](./04_Driver_Examples.md) - Analysis of existing drivers
- [中文规范文档](./05_Specification_CN.md) - Complete Chinese specification
