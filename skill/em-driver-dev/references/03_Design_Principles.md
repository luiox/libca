# Design Principles and Best Practices

This document provides the design principles, naming conventions, and best practices for em_driver development. It serves as a reference for making consistent design decisions and avoiding common pitfalls.

## Table of Contents

1. [Naming Conventions](#naming-conventions)
2. [OOP Style Usage](#oop-style-usage)
3. [Port Layer Design Principles](#port-layer-design-principles)
4. [Error Handling Patterns](#error-handling-patterns)
5. [Memory Management](#memory-management)
6. [Code Organization](#code-organization)
7. [Common Pitfalls](#common-pitfalls)
8. [Performance Considerations](#performance-considerations)

---

## Naming Conventions

### Type Names

| Entity | Convention | Example |
|--------|-----------|---------|
| Device object | `xxx_t` | `led_t`, `bh1750_t`, `dht11_t` |
| Port structure | `xxx_port_t` | `led_port_t`, `bh1750_port_t` |
| Handle types | Use platform handles | `void* gpio`, `void* hi2c` |

### Functions

| Function Type | Convention | Example |
|---------------|-----------|---------|
| Port binding | `xxx_bind_port()` | `led_bind_port()` |
| Port check | `xxx_port_is_registered()` | `led_port_is_registered()` |
| Initialization | `xxx_init()` | `led_init()` |
| Operation | `xxx_operation()` | `led_on()`, `bh1750_read()` |

**Function Signature Pattern**:
```c
// All API functions start with prefix + action
void xxx_init(xxx_t* self, ...);
i32  xxx_read(xxx_t* self, ...);
void xxx_reset(xxx_t* self);
```

### Macros

| Macro Type | Convention | Example |
|------------|-----------|---------|
| Access macros | `XXX_OPERATION()` | `XXX_WRITE()`, `XXX_READ()` |
| Error codes | `XXX_ERR_NAME` | `XXX_ERR_TIMEOUT`, `XXX_ERR_NULL` |
| Success code | `XXX_OK` | `BH1750_OK` |

**All UPPERCASE for macros!**

### Error Codes

| Rule | Example |
|------|---------|
| Use negative numbers for errors | `#define XXX_ERR_TIMEOUT (-1)` |
| Use 0 for success | `#define XXX_OK 0` |
| Group related errors | `#define XXX_ERR_I2C_NACK (-2)` |
| Be descriptive | `#define XXX_ERR_NOT_INITIALIZED (-3)` |

---

## OOP Style Usage

### When to Use OOP Style

**Use OOP style when**:
- Driver manages device state (configuration, mode, data)
- Multiple instances of the same device are needed
- Device has associated hardware resources (pins, handles)

**Examples**:
```c
// OOP style - good for stateful devices
led_t led1, led2;
led_init(&led1, GPIOA, GPIO_PIN_5, 1);
led_init(&led2, GPIOA, GPIO_PIN_6, 0);
led_on(&led1);
led_off(&led2);

bh1750 sensor1, sensor2;
bh1750_init(&sensor1, &hi2c1, 0x46);
bh1750_init(&sensor2, &hi2c1, 0x47);
```

### When NOT to Use OOP Style

**Don't use OOP style when**:
- Function is a pure utility (no state)
- Single global resource (only one instance possible)
- Calculation/conversion function

**Examples**:
```c
// Utility function - NO OOP needed
u32 calculate_crc(const u8* data, u32 length) {
    u32 crc = 0xFFFFFFFF;
    for (u32 i = 0; i < length; i++) {
        crc ^= data[i];
        for (u8 j = 0; j < 8; j++) {
            if (crc & 1) crc = (crc >> 1) ^ 0xEDB88320;
            else crc >>= 1;
        }
    }
    return crc;
}

// Usage - direct call
u32 crc_value = calculate_crc(data_buffer, data_length);
```

### OOP Function Design

**All API functions must have `xxx_t* self` as first parameter**:

```c
// Correct - OOP style
void xxx_function(xxx_t* self, u8 param1, u16 param2) {
    param_check(self != NULL);
    // implementation
}

// Incorrect - procedural style
void xxx_function(u8 param1, u16 param2) {
    // implementation
}
```

---

## Port Layer Design Principles

### Principle 1: Minimal Hardware Abstraction

**Rule**: Include only the minimum necessary functions in the port layer.

**Why**:
- Reduces coupling to specific HAL implementations
- Makes driver testing easier
- Keeps port structure simple

**Example - Good**:
```c
typedef struct led_port {
    void (*write_pin)(void* gpio, u16 pin, u8 value);  // Only what's needed
} led_port_t;
```

**Example - Bad**:
```c
typedef struct led_port {
    void (*write_pin)(void* gpio, u16 pin, u8 value);
    void (*read_pin)(void* gpio, u16 pin);            // Not needed!
    void (*toggle_pin)(void* gpio, u16 pin);          // Not needed!
    void (*init_gpio)(void* gpio);                    // Not needed!
} led_port_t;
```

### Principle 2: Use void* for Handles

**Rule**: Use `void*` for hardware handles to avoid platform dependencies.

**Why**:
- Driver code remains platform-independent
- Can work with different HAL implementations
- More flexible

**Example**:
```c
typedef struct bh1750 {
    void* hi2c;       // Works with any I2C handle type
    u16 dev_addr;
} bh1750_t;

// Usage with STM32 HAL
bh1750_init(&sensor, &hi2c1, 0x46);

// Usage with Arduino
bh1750_init(&sensor, &Wire, 0x46);

// Usage with custom implementation
bh1750_init(&sensor, my_i2c_handle, 0x46);
```

### Principle 3: Group Related Functions

**Rule**: Organize port functions logically.

**Example**:
```c
typedef struct dht11_port {
    // GPIO control
    void (*write_pin)(void* gpio, u16 pin, u8 value);
    u8  (*read_pin)(void* gpio, u16 pin);

    // GPIO configuration
    void (*set_output_mode)(void* gpio, u16 pin);
    void (*set_input_mode)(void* gpio, u16 pin);

    // Timing
    void (*delay_us)(u32 us);
    void (*delay_ms)(u32 ms);
} dht11_port_t;
```

### Principle 4: Single Global Port

**Rule**: Use a single global port pointer for most drivers.

**Why**:
- Simpler implementation
- Reduces memory usage
- Most devices share the same communication interface

**Pattern**:
```c
static const xxx_port_t* g_xxx_port = NULL;

void xxx_bind_port(const xxx_port_t* port) {
    g_xxx_port = port;
}

bool xxx_port_is_registered(void) {
    return g_xxx_port != NULL;
}
```

**Exception**: If the driver truly needs multiple independent ports (rare), you can pass port with each function call.

---

## Error Handling Patterns

### Pattern 1: Port Not Registered

Always check if port is registered before using it:

```c
i32 xxx_read(xxx_t* self, u8* data) {
    // Check port first
    if (!g_xxx_port) {
        debug_print("[xxx] error: port not registered\n");
        return XXX_ERR_PORT_NOT_REGISTERED;
    }

    // Continue with implementation
    // ...
}
```

### Pattern 2: Parameter Validation

Use `param_check` for parameter validation:

```c
i32 xxx_read(xxx_t* self, u8* data, u16 length) {
    // Check port
    if (!g_xxx_port) {
        return XXX_ERR_PORT_NOT_REGISTERED;
    }

    // Validate parameters
    param_check(self != NULL);
    param_check(data != NULL);
    param_check(length > 0);

    // Continue with implementation
    // ...
}
```

### Pattern 3: Initialization State

Check if device is initialized:

```c
i32 xxx_read(xxx_t* self, u8* data) {
    // Check port
    if (!g_xxx_port) return XXX_ERR_PORT_NOT_REGISTERED;

    // Validate parameters
    param_check(self != NULL);
    param_check(data != NULL);

    // Check initialization
    if (!self->initialized) {
        debug_print("[xxx] error: not initialized\n");
        return XXX_ERR_NOT_INITIALIZED;
    }

    // Continue with implementation
    // ...
}
```

### Pattern 4: Hardware Errors

Return appropriate error codes for hardware issues:

```c
i32 xxx_read(xxx_t* self, u8* data, u32 timeout_ms) {
    // ... validation ...

    // Perform hardware operation
    u32 start_time = get_tick();
    while (!xxx_is_data_ready()) {
        if (get_tick() - start_time > timeout_ms) {
            debug_print("[xxx] error: timeout\n");
            return XXX_ERR_TIMEOUT;
        }
    }

    *data = xxx_read_data();
    return XXX_OK;
}
```

### Error Handling Checklist

- [ ] Port registered?
- [ ] Parameters valid?
- [ ] Device initialized?
- [ ] Hardware operation timeout?
- [ ] Return meaningful error codes?
- [ ] Use debug_print for errors?

---

## Memory Management

### Principle 1: No Dynamic Allocation

**Rule**: Do NOT use `malloc`, `free`, or any dynamic memory allocation in drivers.

**Why**:
- Embedded systems have limited heap
- Avoid fragmentation issues
- Deterministic behavior

**Correct**:
```c
// Caller allocates memory
u8 buffer[128];
xxx_read(&device, buffer, sizeof(buffer));
```

**Incorrect**:
```c
// Driver allocates memory - AVOID THIS
u8* xxx_read(void) {
    u8* buffer = malloc(128);  // Don't do this!
    // ...
    return buffer;
}
```

### Principle 2: Stack Allocation

**Rule**: Use stack allocation for temporary buffers.

**Example**:
```c
i32 xxx_read_register(xxx_t* self, u8 reg, u8* value) {
    // Stack allocation for temporary data
    u8 buffer[2] = {reg, 0};  // Register address + dummy

    // Use buffer
    xxx_write_bytes(self, buffer, 2);
    *value = buffer[1];

    return XXX_OK;
}
```

### Principle 3: Caller Provides Memory

**Rule**: Let the caller provide buffers for data.

**Example**:
```c
// Function signature - caller provides buffer
i32 xxx_read_data(xxx_t* self, u8* buffer, u16 max_len);

// Usage - caller manages memory
u8 data[256];
u16 len = xxx_read_data(&device, data, sizeof(data));
```

---

## Code Organization

### File Structure

```
xxx.h:
  1. License and header guards
  2. Includes
  3. Forward declarations
  4. Port structure definition
  5. Device structure definition
  6. Port binding functions
  7. API function declarations
  8. Error codes
  9. Header guard close

xxx.c:
  1. Includes
  2. Static global port pointer
  3. Port binding implementation
  4. Access macros (if any)
  5. API function implementations
```

### Header File Template

```c
#ifndef LIBCA_EM_DRIVER_XXX_H
#define LIBCA_EM_DRIVER_XXX_H

#include "em_base/datatype.h"

#ifdef __cplusplus
extern "C" {
#endif

// Forward declarations
typedef struct xxx_port xxx_port_t;
typedef struct xxx xxx_t;

// Port structure
struct xxx_port {
    // Function pointers...
};

// Device structure
struct xxx {
    // Members...
};

// Port binding
void xxx_bind_port(const xxx_port_t* port);
bool xxx_port_is_registered(void);

// API functions
void xxx_init(xxx_t* self, ...);
i32  xxx_read(xxx_t* self, ...);

// Error codes
#define XXX_OK       0
#define XXX_ERR_XXX (-1)

#ifdef __cplusplus
}
#endif

#endif // LIBCA_EM_DRIVER_XXX_H
```

### Implementation File Template

```c
#include "../em_base/debug.h"
#include "xxx.h"

// Global port pointer
static const xxx_port_t* g_xxx_port = NULL;

// Access macros (optional)
#define XXX_WRITE(v) g_xxx_port->write((v))

// Port binding
void xxx_bind_port(const xxx_port_t* port) {
    g_xxx_port = port;
}

bool xxx_port_is_registered(void) {
    return g_xxx_port != NULL;
}

// API implementation
void xxx_init(xxx_t* self, ...) {
    param_check(self != NULL);
    // ...
}
```

---

## Common Pitfalls

### Pitfall 1: Forgetting to Bind Port

**Symptoms**: `port not registered` error, crashes

**Solution**: Always call `xxx_bind_port()` before using driver

```c
// Incorrect - forgot to bind
xxx_t device;
xxx_init(&device, ...);
xxx_read(&device, &data);  // ERROR!

// Correct - bind before use
xxx_t device;
xxx_bind_port(&port);
xxx_init(&device, ...);
xxx_read(&device, &data);  // OK
```

### Pitfall 2: Using Wrong Delay Precision

**Symptoms**: Timing-sensitive devices fail (DHT11, One-Wire)

**Solution**: Use microsecond delays for precise timing

```c
// Incorrect - millisecond delay for microsecond timing
delay_ms(1);  // Too coarse for DHT11

// Correct - microsecond delay
delay_us(40);  // Proper precision
```

### Pitfall 3: Ignoring Error Codes

**Symptoms**: Silent failures, incorrect behavior

**Solution**: Always check return values

```c
// Incorrect - ignore errors
xxx_read(&device, &data);
process(data);  // Data might be invalid

// Correct - check errors
i32 result = xxx_read(&device, &data);
if (result == XXX_OK) {
    process(data);
} else {
    handle_error(result);
}
```

### Pitfall 4: Not Verifying I2C Addresses

**Symptoms**: I2C NACK, communication fails

**Solution**: Verify 7-bit vs 8-bit addresses

```c
// Incorrect - using 8-bit address with HAL
u16 addr = 0xA0;  // 8-bit address
HAL_I2C_Master_Transmit(hi2c, addr, ...);  // Wrong!

// Correct - convert to 7-bit address
u16 addr = 0xA0 >> 1;  // 7-bit address
HAL_I2C_Master_Transmit(hi2c, addr, ...);  // Correct
```

### Pitfall 5: Not Checking Array Bounds

**Symptoms**: Buffer overflows, crashes

**Solution**: Always validate lengths

```c
// Incorrect - no bounds checking
void xxx_write(xxx_t* self, u8* data, u16 len) {
    for (u16 i = 0; i < len; i++) {
        self->buffer[i] = data[i];  // Overflow risk!
    }
}

// Correct - check bounds
void xxx_write(xxx_t* self, u8* data, u16 len) {
    param_check(len <= self->buffer_size);
    for (u16 i = 0; i < len; i++) {
        self->buffer[i] = data[i];
    }
}
```

### Pitfall 6: Forgetting Write Cycle Time (EEPROM)

**Symptoms**: Data not written correctly, read back old data

**Solution**: Wait for write cycle to complete

```c
// Incorrect - no delay after write
at24c_write(&eeprom, addr, data, len);
at24c_read(&eeprom, addr, read_buf, len);  // Data not written yet!

// Correct - wait for write cycle
at24c_write(&eeprom, addr, data, len);
delay_ms(10);  // Wait for write cycle
at24c_read(&eeprom, addr, read_buf, len);  // OK
```

---

## Performance Considerations

### Optimize I2C Operations

**Problem**: Multiple small I2C transactions are slow

**Solution**: Combine operations when possible

```c
// Slow - multiple transactions
xxx_write_register(device, REG_CONFIG, config);
xxx_write_register(device, REG_MODE, mode);
xxx_write_register(device, REG_THRESHOLD, threshold);

// Fast - single transaction
u8 buffer[3] = {REG_CONFIG, REG_MODE, REG_THRESHOLD};
u8 values[3] = {config, mode, threshold};
xxx_write_multi(device, buffer, values, 3);
```

### Cache Computed Values

**Problem**: Recalculating values wastes CPU time

**Solution**: Cache results when possible

```c
// Slow - recalculate every time
f32 get_temperature(xxx_t* self) {
    u16 raw = read_raw_adc();
    return (raw * 3.3f / 4096.0f - 0.5f) * 100.0f;  // Complex math
}

// Fast - cache last value
f32 get_temperature(xxx_t* self) {
    if (self->cache_valid) {
        return self->cached_temp;  // Use cached value
    }
    u16 raw = read_raw_adc();
    self->cached_temp = (raw * 3.3f / 4096.0f - 0.5f) * 100.0f;
    self->cache_valid = 1;
    return self->cached_temp;
}
```

### Minimize Delays

**Problem**: Unnecessary delays block execution

**Solution**: Use polling instead of fixed delays when possible

```c
// Slow - fixed delay
xxx_start_conversion(device);
delay_ms(50);  // Always wait 50ms
u8 data = xxx_read_data(device);

// Fast - poll for ready
xxx_start_conversion(device);
while (!xxx_is_ready(device));  // Wait only as long as needed
u8 data = xxx_read_data(device);
```

### Use Inline Functions for Small Operations

**Problem**: Function call overhead for trivial operations

**Solution**: Use inline or macros for small functions

```c
// Slower - function call overhead
void xxx_set_bit(xxx_t* self, u8 bit) {
    self->reg |= (1 << bit);
}

// Faster - inline
static inline void xxx_set_bit(xxx_t* self, u8 bit) {
    self->reg |= (1 << bit);
}

// Or use macro
#define XXX_SET_BIT(self, bit) ((self)->reg |= (1 << (bit)))
```

---

## Design Checklist

Before finalizing a driver, verify:

**Structure**:
- [ ] Port structure has only necessary functions
- [ ] Device object follows member organization (handles, config, state)
- [ ] All API functions have `xxx_t* self` as first parameter
- [ ] Port binding functions exist

**Naming**:
- [ ] Types use `xxx_t` suffix
- [ ] Functions use `xxx_` prefix
- [ ] Macros are UPPERCASE
- [ ] Error codes are negative (except 0 for success)

**Error Handling**:
- [ ] Port registration checked before use
- [ ] Parameters validated with `param_check`
- [ ] Initialization state checked (if applicable)
- [ ] Meaningful error codes returned
- [ ] `debug_print` used for error logging

**Memory**:
- [ ] No dynamic allocation (malloc/free)
- [ ] Stack allocation used for temporary buffers
- [ ] Caller provides data buffers

**Documentation**:
- [ ] Header file has clear comments
- [ ] Function parameters documented
- [ ] Error codes explained
- [ ] Usage example provided

## Related Documents

- [Driver Development Workflow](./01_Driver_Development_Workflow.md) - Step-by-step guide
- [Common Driver Patterns](./02_Common_Driver_Patterns.md) - Detailed examples
- [Driver Examples](./04_Driver_Examples.md) - Analysis of existing drivers
