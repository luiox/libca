# Common Driver Patterns

This document provides detailed examples of common driver patterns found in the libca em_driver module. Each pattern includes the port design rationale, device object structure, key implementation details, and usage examples.

## Table of Contents

1. [Simple GPIO Driver (LED)](#1-simple-gpio-driver-led)
2. [I2C Sensor Driver (BH1750)](#2-i2c-sensor-driver-bh1750)
3. [Timing-Sensitive Driver (DHT11)](#3-timing-sensitive-driver-dht11)
4. [State Machine Driver (EC11 Encoder)](#4-state-machine-driver-ec11-encoder)
5. [EEPROM Driver (AT24CXX)](#5-eeprom-driver-at24cxx)

---

## 1. Simple GPIO Driver (LED)

### Overview

A simple GPIO driver controls a single pin's output state. This is the simplest driver pattern and serves as a good starting point for understanding the em_driver framework.

### Port Design

```c
typedef struct led_port {
    void (*write_pin)(void* gpio, u16 pin, u8 value);
} led_port_t;
```

**Rationale**:
- Only needs to write to a GPIO pin
- No need for reading (output-only device)
- No timing requirements
- Minimal hardware abstraction

### Device Object

```c
typedef struct led {
    void* gpio;       // GPIO handle (platform-specific)
    u16 pin;          // Pin number
    u8 active_level;  // Active level (0=low active, 1=high active)
} led_t;
```

**Member Explanation**:
- `gpio`: Platform-specific GPIO port handle (e.g., `GPIOA` pointer)
- `pin`: Pin number (e.g., `GPIO_PIN_5`)
- `active_level`: Whether the LED turns on with HIGH or LOW signal

### API Functions

```c
void led_init(led_t* self, void* gpio, u16 pin, u8 active_level);
void led_on(led_t* self);
void led_off(led_t* self);
void led_toggle(led_t* self);
```

### Implementation

```c
#include "../em_base/debug.h"

static const led_port_t* g_led_port = NULL;

void led_bind_port(const led_port_t* port) {
    g_led_port = port;
}

bool led_port_is_registered(void) {
    return g_led_port != NULL;
}

void led_init(led_t* self, void* gpio, u16 pin, u8 active_level) {
    self->gpio = gpio;
    self->pin = pin;
    self->active_level = active_level;
}

void led_on(led_t* self) {
    param_check(self != NULL);
    if (!g_led_port) {
        debug_print("[led] error: port not registered\n");
        return;
    }
    g_led_port->write_pin(self->gpio, self->pin, self->active_level);
}

void led_off(led_t* self) {
    param_check(self != NULL);
    if (!g_led_port) {
        debug_print("[led] error: port not registered\n");
        return;
    }
    g_led_port->write_pin(self->gpio, self->pin, !self->active_level);
}

void led_toggle(led_t* self) {
    param_check(self != NULL);
    if (!g_led_port) {
        debug_print("[led] error: port not registered\n");
        return;
    }
    // Read current state and toggle
    // Note: This requires read_pin capability, which LED doesn't have
    // Alternative: track state in led_t
}
```

### Usage Example

```c
// On STM32 with HAL
led_t status_led;
led_bind_port(&(led_port_t){
    .write_pin = HAL_GPIO_WritePin
});

led_init(&status_led, GPIOA, GPIO_PIN_5, 1);  // High active

led_on(&status_led);   // Turn on
HAL_Delay(500);
led_off(&status_led);  // Turn off
```

### Key Takeaways

- Simplest possible driver pattern
- Demonstrates basic port binding
- Shows `param_check` usage
- Active level abstraction for flexibility

---

## 2. I2C Sensor Driver (BH1750)

### Overview

BH1750 is a digital light sensor that communicates via I2C. It demonstrates how to handle I2C communication, device addressing, and register-based configuration.

### Port Design

```c
typedef struct bh1750_port {
    i32 (*i2c_write)(void* hi2c, u16 dev_addr, u16 mem_addr,
                     u16 mem_addr_size, u8* data, u16 data_size, u32 timeout);
    i32 (*i2c_read)(void* hi2c, u16 dev_addr, u16 mem_addr,
                    u16 mem_addr_size, u8* data, u16 data_size, u32 timeout);
} bh1750_port_t;
```

**Rationale**:
- Requires both I2C read and write operations
- Uses standard HAL I2C function signature
- Supports multiple I2C addresses (0x46 or 0x47)
- No timing functions needed (I2C handles this)

### Device Object

```c
typedef struct bh1750 {
    void* hi2c;       // I2C handle
    u16 dev_addr;     // I2C device address (0x46 or 0x47)
} bh1750_t;
```

**Member Explanation**:
- `hi2c`: I2C peripheral handle (e.g., `&hi2c1`)
- `dev_addr`: 7-bit I2C address (depends on ADDR pin state)

### Access Macros

```c
#define BH1750_I2C_WRITE(reg, data, size) \
    g_bh1750_port->i2c_write((self)->hi2c, (self)->dev_addr, (reg), 1, \
                             (data), (size), 1000)

#define BH1750_I2C_READ(reg, buf, size) \
    g_bh1750_port->i2c_read((self)->hi2c, (self)->dev_addr, (reg), 1, \
                            (buf), (size), 1000)
```

### API Functions

```c
void bh1750_init(bh1750_t* self, void* hi2c, u16 dev_addr);
i32  bh1750_read_light(bh1750_t* self, f32* lux);
```

### Implementation

```c
#include "../em_base/debug.h"
#include <math.h>

static const bh1750_port_t* g_bh1750_port = NULL;

void bh1750_bind_port(const bh1750_port_t* port) {
    g_bh1750_port = port;
}

void bh1750_init(bh1750_t* self, void* hi2c, u16 dev_addr) {
    self->hi2c = hi2c;
    self->dev_addr = dev_addr;

    // Power on and set measurement mode
    u8 cmd = 0x01;  // Power on
    if (!g_bh1750_port) {
        debug_print("[bh1750] error: port not registered\n");
        return;
    }
    g_bh1750_port->i2c_write(hi2c, dev_addr, 0, 0, &cmd, 1, 1000);
}

i32 bh1750_read_light(bh1750_t* self, f32* lux) {
    param_check(self != NULL);
    param_check(lux != NULL);

    if (!g_bh1750_port) {
        debug_print("[bh1750] error: port not registered\n");
        return -1;
    }

    // Start continuous measurement (high resolution mode)
    u8 cmd = 0x10;
    g_bh1750_port->i2c_write(self->hi2c, self->dev_addr, 0, 0, &cmd, 1, 1000);

    // Wait for measurement
    // (I2C polling delay is handled by the I2C implementation)

    // Read result (2 bytes)
    u8 data[2];
    g_bh1750_port->i2c_read(self->hi2c, self->dev_addr, 0, 0, data, 2, 1000);

    // Convert to lux
    u16 raw = (data[0] << 8) | data[1];
    *lux = raw / 1.2f;  // Conversion factor from datasheet

    return 0;
}
```

### Usage Example

```c
bh1750 light_sensor;
bh1750_bind_port(&(bh1750_port_t){
    .i2c_write = HAL_I2C_Master_Transmit,
    .i2c_read = HAL_I2C_Master_Receive
});

bh1750_init(&light_sensor, &hi2c1, 0x46);  // ADDR pin low

f32 lux;
bh1750_read_light(&light_sensor, &lux);
printf("Light intensity: %.1f lux\n", lux);
```

### Key Takeaways

- Demonstrates I2C communication pattern
- Shows register-based device control
- Uses access macros for cleaner code
- Conversion from raw data to physical units

---

## 3. Timing-Sensitive Driver (DHT11)

### Overview

DHT11 is a temperature and humidity sensor that requires precise timing (microsecond-level delays). It demonstrates bit-banging communication and bidirectional GPIO usage.

### Port Design

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

**Rationale**:
- **Bidirectional GPIO**: Needs to switch between output and input modes
- **Precise timing**: Microsecond-level delays required for protocol
- **Pin control**: Write for start signal, read for data bits
- **Millisecond delays**: Between measurements (sensor needs recovery time)

### Device Object

```c
typedef struct dht11 {
    void* gpio;       // GPIO handle
    u16 pin;          // Pin number
    u8 last_valid;    // Last read was successful
} dht11_t;
```

### Access Macros

```c
#define DHT11_OUTPUT_MODE(self) \
    g_dht11_port->set_output_mode((self)->gpio, (self)->pin)

#define DHT11_INPUT_MODE(self) \
    g_dht11_port->set_input_mode((self)->gpio, (self)->pin)

#define DHT11_WRITE(self, v) \
    g_dht11_port->write_pin((self)->gpio, (self)->pin, (v))

#define DHT11_READ(self) \
    g_dht11_port->read_pin((self)->gpio, (self)->pin)

#define DHT11_DELAY_US(us) \
    g_dht11_port->delay_us(us)

#define DHT11_DELAY_MS(ms) \
    g_dht11_port->delay_ms(ms)
```

### API Functions

```c
void dht11_init(dht11_t* self, void* gpio, u16 pin);
i32  dht11_read(dht11_t* self, u8* temp, u8* humi);
```

### Implementation

```c
#include "../em_base/debug.h"

static const dht11_port_t* g_dht11_port = NULL;

void dht11_bind_port(const dht11_port_t* port) {
    g_dht11_port = port;
}

void dht11_init(dht11_t* self, void* gpio, u16 pin) {
    self->gpio = gpio;
    self->pin = pin;
    self->last_valid = 0;
}

i32 dht11_read(dht11_t* self, u8* temp, u8* humi) {
    param_check(self != NULL);
    param_check(temp != NULL);
    param_check(humi != NULL);

    if (!g_dht11_port) {
        debug_print("[dht11] error: port not registered\n");
        return -1;
    }

    // 1. Send start signal (host pulls low for >18ms)
    DHT11_OUTPUT_MODE(self);
    DHT11_WRITE(self, 0);
    DHT11_DELAY_MS(20);

    // 2. Pull high and wait for response
    DHT11_WRITE(self, 1);
    DHT11_INPUT_MODE(self);

    // Wait for sensor to pull low (should be ~20-40us)
    u32 timeout = 100;
    while (DHT11_READ(self) && timeout-- > 0) DHT11_DELAY_US(1);
    if (timeout == 0) {
        debug_print("[dht11] timeout waiting for sensor response\n");
        return -1;
    }

    // Wait for sensor to pull high (should be ~80us)
    timeout = 100;
    while (!DHT11_READ(self) && timeout-- > 0) DHT11_DELAY_US(1);
    if (timeout == 0) {
        debug_print("[dht11] timeout waiting for sensor high\n");
        return -1;
    }

    // 3. Read 40 bits of data
    u8 data[5] = {0};
    for (u8 i = 0; i < 5; i++) {
        for (u8 j = 0; j < 8; j++) {
            // Wait for bit start (low)
            timeout = 100;
            while (DHT11_READ(self) && timeout-- > 0) DHT11_DELAY_US(1);
            timeout = 100;
            while (!DHT11_READ(self) && timeout-- > 0) DHT11_DELAY_US(1);

            // Measure bit duration
            DHT11_DELAY_US(40);  // Wait halfway through bit period

            // If pin is high, it's a '1' (long pulse)
            if (DHT11_READ(self)) {
                data[i] |= (1 << (7 - j));
                // Wait for remaining high time
                timeout = 100;
                while (DHT11_READ(self) && timeout-- > 0) DHT11_DELAY_US(1);
            }
        }
    }

    // 4. Verify checksum
    if (data[4] != (data[0] + data[1] + data[2] + data[3])) {
        debug_print("[dht11] checksum error\n");
        return -1;
    }

    *humi = data[0];  // Humidity integer part
    *temp = data[2];  // Temperature integer part
    self->last_valid = 1;

    return 0;
}
```

### Usage Example

```c
dht11 temp_sensor;
dht11_bind_port(&(dht11_port_t){
    .write_pin = HAL_GPIO_WritePin,
    .read_pin = HAL_GPIO_ReadPin,
    .set_output_mode = HAL_GPIO_SetOutputMode,
    .set_input_mode = HAL_GPIO_SetInputMode,
    .delay_us = HAL_Delay_us,
    .delay_ms = HAL_Delay
});

dht11_init(&temp_sensor, GPIOA, GPIO_PIN_6);

u8 temp, humi;
if (dht11_read(&temp_sensor, &temp, &humi) == 0) {
    printf("Temperature: %d°C, Humidity: %d%%\n", temp, humi);
}
```

### Key Takeaways

- Demonstrates bidirectional GPIO usage
- Shows precise timing control with microsecond delays
- Implements protocol-level bit reading
- Includes checksum verification

---

## 4. State Machine Driver (EC11 Encoder)

### Overview

EC11 is a rotary encoder that requires a state machine to decode rotation direction. It demonstrates how to maintain internal state and handle asynchronous events.

### Port Design

```c
typedef struct ec11_port {
    u8 (*read_a)(void* gpio, u16 pin_a);  // Read pin A
    u8 (*read_b)(void* gpio, u16 pin_b);  // Read pin B
    void (*set_mode)(void* gpio, u16 pin_a, u16 pin_b, u8 mode);
} ec11_port_t;
```

**Rationale**:
- Two input pins (A and B) with quadrature encoding
- Only needs read operations (input device)
- No timing requirements (polling-based)
- State machine tracks rotation direction

### Device Object

```c
typedef struct ec11 {
    void* gpio;       // GPIO handle
    u16 pin_a;        // Pin A
    u16 pin_b;        // Pin B
    u8 state;         // Current state (0-3)
    i16 count;        // Rotation counter
} ec11_t;
```

**Member Explanation**:
- `gpio`: GPIO handle for both pins
- `pin_a`, `pin_b`: Pin numbers for quadrature signals
- `state`: Current state in quadrature sequence (0-3)
- `count`: Cumulative rotation count (positive for CW, negative for CCW)

### API Functions

```c
void ec11_init(ec11_t* self, void* gpio, u16 pin_a, u16 pin_b);
void ec11_update(ec11_t* self);  // Call periodically
i16  ec11_get_count(ec11_t* self);
void ec11_reset_count(ec11_t* self);
```

### Implementation

```c
#include "../em_base/debug.h"

static const ec11_port_t* g_ec11_port = NULL;

void ec11_bind_port(const ec11_port_t* port) {
    g_ec11_port = port;
}

void ec11_init(ec11_t* self, void* gpio, u16 pin_a, u16 pin_b) {
    self->gpio = gpio;
    self->pin_a = pin_a;
    self->pin_b = pin_b;
    self->state = 0;
    self->count = 0;

    // Configure pins as input
    if (g_ec11_port) {
        g_ec11_port->set_mode(gpio, pin_a, pin_b, 1);  // 1 = input
    }
}

void ec11_update(ec11_t* self) {
    param_check(self != NULL);

    if (!g_ec11_port) {
        debug_print("[ec11] error: port not registered\n");
        return;
    }

    // Read current state of both pins
    u8 a = g_ec11_port->read_a(self->gpio, self->pin_a);
    u8 b = g_ec11_port->read_b(self->gpio, self->pin_b);

    // State machine for quadrature decoding
    u8 new_state = (a << 1) | b;

    // State transition table for CW rotation:
    // 00 -> 01 -> 11 -> 10 -> 00
    // State transition table for CCW rotation:
    // 00 -> 10 -> 11 -> 01 -> 00

    // Transition matrix [old_state][new_state]
    // -1: CCW, 0: invalid/no change, +1: CW
    static const i8 transition_table[4][4] = {
        // new state: 00  01  11  10
        /* 00 */ { 0, -1,  0,  1},
        /* 01 */ { 1,  0, -1,  0},
        /* 11 */ { 0,  1,  0, -1},
        /* 10 */ {-1,  0,  1,  0}
    };

    i8 change = transition_table[self->state][new_state];
    self->count += change;
    self->state = new_state;
}

i16 ec11_get_count(ec11_t* self) {
    param_check(self != NULL);
    return self->count;
}

void ec11_reset_count(ec11_t* self) {
    param_check(self != NULL);
    self->count = 0;
}
```

### Usage Example

```c
ec11 rotary_encoder;
ec11_bind_port(&(ec11_port_t){
    .read_a = HAL_GPIO_ReadPin,
    .read_b = HAL_GPIO_ReadPin,
    .set_mode = HAL_GPIO_SetInputMode
});

ec11_init(&rotary_encoder, GPIOA, GPIO_PIN_7, GPIO_PIN_8);

// In main loop
while (1) {
    ec11_update(&rotary_encoder);
    i16 count = ec11_get_count(&rotary_encoder);
    if (count != 0) {
        printf("Encoder count: %d\n", count);
    }
    HAL_Delay(10);  // Poll at 100Hz
}
```

### Key Takeaways

- Demonstrates state machine pattern
- Shows internal state maintenance
- Polling-based event handling
- Quadrature signal decoding

---

## 5. EEPROM Driver (AT24CXX)

### Overview

AT24CXX is a series of I2C EEPROMs with different capacities. It demonstrates I2C communication with address paging and write cycle handling.

### Port Design

```c
typedef struct at24c_port {
    i32 (*i2c_write)(void* hi2c, u16 dev_addr, u16 mem_addr,
                     u16 mem_addr_size, u8* data, u16 data_size, u32 timeout);
    i32 (*i2c_read)(void* hi2c, u16 dev_addr, u16 mem_addr,
                    u16 mem_addr_size, u8* data, u16 data_size, u32 timeout);
    void (*delay_ms)(u32 ms);
} at24c_port_t;
```

**Rationale**:
- Standard I2C operations for read/write
- `delay_ms` needed for write cycle time (EEPROM needs ~5-10ms to complete write)
- No timing requirements for reading

### Device Object

```c
typedef struct at24c {
    void* hi2c;       // I2C handle
    u16 dev_addr;     // Base I2C address
    u16 capacity;     // Total capacity in bytes
    u16 page_size;    // Page size for writes (e.g., 32 bytes)
} at24c_t;
```

### API Functions

```c
void at24c_init(at24c_t* self, void* hi2c, u16 dev_addr, u16 capacity, u16 page_size);
i32  at24c_read(at24c_t* self, u16 addr, u8* data, u16 len);
i32  at24c_write(at24c_t* self, u16 addr, const u8* data, u16 len);
```

### Implementation

```c
#include "../em_base/debug.h"

static const at24c_port_t* g_at24c_port = NULL;

void at24c_bind_port(const at24c_port_t* port) {
    g_at24c_port = port;
}

void at24c_init(at24c_t* self, void* hi2c, u16 dev_addr, u16 capacity, u16 page_size) {
    self->hi2c = hi2c;
    self->dev_addr = dev_addr;
    self->capacity = capacity;
    self->page_size = page_size;
}

i32 at24c_read(at24c_t* self, u16 addr, u8* data, u16 len) {
    param_check(self != NULL);
    param_check(data != NULL);
    param_check(addr + len <= self->capacity);

    if (!g_at24c_port) {
        debug_print("[at24c] error: port not registered\n");
        return -1;
    }

    // Handle I2C address selection for larger EEPROMs
    u16 i2c_addr = self->dev_addr;
    if (self->capacity > 256 * 1024) {
        // Use address bits for devices >256KB
        i2c_addr |= (addr >> 16) & 0x07;
        addr &= 0xFFFF;
    } else if (self->capacity > 32 * 1024) {
        i2c_addr |= (addr >> 15) & 0x07;
        addr &= 0x7FFF;
    }

    return g_at24c_port->i2c_read(self->hi2c, i2c_addr, addr, 2, data, len, 1000);
}

i32 at24c_write(at24c_t* self, u16 addr, const u8* data, u16 len) {
    param_check(self != NULL);
    param_check(data != NULL);
    param_check(addr + len <= self->capacity);

    if (!g_at24c_port) {
        debug_print("[at24c] error: port not registered\n");
        return -1;
    }

    u16 written = 0;
    while (written < len) {
        // Calculate page-aligned write length
        u16 page_start = (addr + written) & ~(self->page_size - 1);
        u16 page_end = page_start + self->page_size;
        u16 write_len = page_end - (addr + written);
        if (write_len > (len - written)) {
            write_len = len - written;
        }

        // Write page
        g_at24c_port->i2c_write(self->hi2c, self->dev_addr, addr + written,
                                 2, (u8*)(data + written), write_len, 1000);

        // Wait for write cycle to complete
        g_at24c_port->delay_ms(10);

        written += write_len;
    }

    return 0;
}
```

### Usage Example

```c
at24c eeprom;
at24c_bind_port(&(at24c_port_t){
    .i2c_write = HAL_I2C_Master_Transmit,
    .i2c_read = HAL_I2C_Master_Receive,
    .delay_ms = HAL_Delay
});

at24c_init(&eeprom, &hi2c1, 0xA0, 32768, 32);  // AT24C256, 32KB, 32-byte pages

// Write some data
u8 write_data[10] = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9};
at24c_write(&eeprom, 0x1000, write_data, 10);

// Read back
u8 read_data[10];
at24c_read(&eeprom, 0x1000, read_data, 10);
```

### Key Takeaways

- Demonstrates page-based write handling
- Shows address selection for large EEPROMs
- Includes write cycle timing
- Handles boundary conditions (page alignment, capacity check)

---

## Pattern Selection Guide

| Requirement | Recommended Pattern | Example |
|-------------|---------------------|---------|
| Single output control | Simple GPIO | LED, relay, buzzer |
| Register-based sensor | I2C Sensor | BH1750, MPU6050 |
| Precise timing protocol | Timing-Sensitive | DHT11, One-Wire |
| Quadrature input | State Machine | EC11 encoder |
| Non-volatile storage | EEPROM | AT24CXX |
| Complex state machine | State Machine + Timing | RF modules |

## Common Issues and Solutions

### Issue 1: Port Not Registered

**Symptoms**: `port not registered` error message

**Solution**: Always call `xxx_bind_port()` before using any API function

### Issue 2: Timing Problems

**Symptoms**: Read/write failures, incorrect data

**Solution**:
- Verify delay functions are accurate (use oscilloscope if possible)
- Check datasheet for minimum/maximum timing requirements
- Consider hardware timing variations between MCUs

### Issue 3: I2C Address Issues

**Symptoms**: NACK on I2C bus

**Solution**:
- Verify device address (7-bit vs 8-bit)
- Check address pins on hardware
- Use I2C scanner to detect devices on bus

### Issue 4: State Machine Glitches

**Symptoms**: Incorrect rotation direction or missed counts

**Solution**:
- Poll at appropriate frequency (too slow: miss events, too fast: bounce)
- Add debouncing if necessary
- Verify signal quality (oscilloscope)

## Related Documents

- [Driver Development Workflow](./01_Driver_Development_Workflow.md) - Step-by-step guide
- [Design Principles](./03_Design_Principles.md) - Best practices
- [Driver Examples](./04_Driver_Examples.md) - More detailed analysis
