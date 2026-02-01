---
name: em-driver-dev
description: This skill should be used when developing em_driver hardware drivers for embedded MCU systems. Use it for creating new sensor/peripheral drivers, implementing port layer abstractions, and following the OOP-style driver pattern defined in the libca project. Triggered by requests like "develop a xxx driver" or "implement a sensor driver for [device name]".
---

# EM Driver Development Guide

## Overview

This skill guides the development of em_driver hardware drivers for embedded MCU systems. It enforces the standardized OOP-style driver pattern with port layer abstraction, ensuring consistent code structure and hardware independence across the libca project.

## Core Principles

### 1. OOP-Style Pattern
- All API functions have `xxx_t* self` as first parameter
- Device object manages hardware resources and state
- Port layer abstracts hardware-specific operations

### 2. Port Layer Abstraction
- Single global port pointer for most drivers
- Function pointers for hardware operations
- Use `void*` for platform-independent handles

### 3. Consistent Naming
- Types: `xxx_t`, `xxx_port_t`
- Functions: `xxx_bind_port()`, `xxx_init()`, `xxx_operation()`
- Macros: `XXX_WRITE()`, `XXX_READ()`, `XXX_OK`, `XXX_ERR_*`

### 4. No Unit Tests
- Drivers depend on hardware
- Test on real MCU or with simulated port

## Quick Start

### Developing a New Driver

1. **Analyze hardware** (communication interface, port functions, configuration)
2. **Define structures** (port layer, device object, API)
3. **Implement logic** (port binding, error handling, device operations)
4. **Integrate build** (add to xmake.lua)
5. **Test** (simulation first, then hardware)

See: [01_Driver_Development_Workflow.md](./references/01_Driver_Development_Workflow.md)

### Common Driver Patterns

| Pattern | Use Case | Example |
|---------|----------|---------|
| Simple GPIO | Output control (LED, relay) | [LED](./references/04_Driver_Examples.md#led) |
| I2C Sensor | Register-based sensor | [BH1750](./references/04_Driver_Examples.md#bh1750) |
| Timing-Sensitive | Bit-banging protocol | [DHT11](./references/04_Driver_Examples.md#dht11) |
| State Machine | Quadrature encoder | [EC11](./references/04_Driver_Examples.md#ec11) |
| EEPROM | Non-volatile storage | [AT24CXX](./references/04_Driver_Examples.md#at24cxx) |

See: [02_Common_Driver_Patterns.md](./references/02_Common_Driver_Patterns.md)

## Essential Patterns

### Port Binding
```c
static const xxx_port_t* g_xxx_port = NULL;

void xxx_bind_port(const xxx_port_t* port) {
    g_xxx_port = port;
}

bool xxx_port_is_registered(void) {
    return g_xxx_port != NULL;
}
```

### Error Handling
```c
i32 xxx_read(xxx_t* self, u8* data) {
    if (!g_xxx_port) {
        debug_print("[xxx] error: port not registered\n");
        return XXX_ERR_PORT_NOT_REGISTERED;
    }
    param_check(self != NULL);
    param_check(data != NULL);
    // ... implementation
    return XXX_OK;
}
```

### Access Macros
```c
#define XXX_WRITE(self, v)    g_xxx_port->write((self)->gpio, (self)->pin, (v))
#define XXX_READ(self)        g_xxx_port->read((self)->gpio, (self)->pin)
```

## Resources

### References (Detailed Guides)

1. **[01_Driver_Development_Workflow.md](./references/01_Driver_Development_Workflow.md)**
   - Complete step-by-step driver development guide
   - Hardware requirement analysis
   - Structure definition patterns
   - Build system integration
   - Testing approaches
   - Maintenance workflow

2. **[02_Common_Driver_Patterns.md](./references/02_Common_Driver_Patterns.md)**
   - 5 common driver patterns with detailed examples
   - LED, BH1750, DHT11, EC11, AT24CXX
   - Port design rationale
   - Implementation details
   - Usage examples

3. **[03_Design_Principles.md](./references/03_Design_Principles.md)**
   - Naming conventions
   - OOP style usage guidelines
   - Port layer design principles
   - Error handling patterns
   - Memory management rules
   - Common pitfalls and solutions
   - Performance considerations

4. **[04_Driver_Examples.md](./references/04_Driver_Examples.md)**
   - Analysis of existing drivers in the codebase
   - Real-world implementation examples
   - Code snippets and explanations

5. **[05_Specification_CN.md](./references/05_Specification_CN.md)**
   - Complete Chinese specification document
   - Detailed coding standards
   - Best practices and examples

### Quick Reference

#### File Structure
```
xxx.h:  Port, Device, API declarations, Error codes
xxx.c:  Port binding, Access macros, Implementation
```

#### Naming Cheat Sheet
```
Types:      xxx_t, xxx_port_t
Functions:  xxx_bind_port(), xxx_init(), xxx_read()
Macros:     XXX_WRITE(), XXX_OK, XXX_ERR_TIMEOUT
```

#### Error Code Pattern
```
#define XXX_OK                     0
#define XXX_ERR_PORT_NOT_REGISTERED (-1)
#define XXX_ERR_INVALID_PARAM       (-2)
#define XXX_ERR_TIMEOUT             (-3)
// All errors are negative!
```

## Common Issues

| Issue | Solution |
|-------|----------|
| Port not registered | Call `xxx_bind_port()` before use |
| Timing errors | Use microsecond delays for precision |
| I2C NACK | Verify 7-bit vs 8-bit address |
| Data corruption | Wait for write cycle (EEPROM) |
| Wrong data type | Check datasheet for data format |

For detailed troubleshooting, see [Common Pitfalls](./references/03_Design_Principles.md#common-pitfalls).

## When NOT to Use OOP Style

For utility functions without state management:
```c
// NO OOP - pure calculation
u32 calculate_crc(const u8* data, u32 length) {
    // Implementation
    return crc_value;
}
```

See [Design Principles](./references/03_Design_Principles.md#oop-style-usage) for details.

---

**Note**: This skill does NOT require unit tests. Drivers are tested in real hardware or simulation environments.
