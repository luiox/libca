---
name: em-component-dev
description: This skill should be used when developing em_util/em_base general-purpose components for the libca embedded project. Use it for creating algorithms, data structures, utility functions, and other hardware-independent modules that require unit testing. Triggered by requests like "implement a xxx algorithm", "create a xxx data structure", or "develop a xxx utility function".
---

# EM Component Development Guide

## Overview

This skill guides the development of general-purpose components (em_util/em_base) for the libca embedded project. These components are hardware-independent, designed for computer simulation, and require comprehensive unit testing.

## Component Development Workflow

### Step 1: Design the Component

Before coding, clarify:

1. **Component type**:
   - **Algorithm**: CRC, filters, math utilities, PID controllers
   - **Container**: Ring buffer, queue, stack, linked list
   - **Utility**: Bit operations, endian conversion, memory pool
   - **Other**: Custom logic that doesn't depend on hardware

2. **Public API design**:
   - Function names and parameters
   - Return values and error handling
   - Data structures needed
   - Performance requirements

3. **Test requirements**:
   - Standard test vectors (for algorithms like CRC)
   - Boundary conditions (0, 1, MAX_SIZE)
   - Error scenarios (if applicable)
   - Edge cases

### Step 2: Create Component Files

#### 2.1 Header File (xxx.h)

Structure:

```c
#ifndef LIBCA_EM_UTIL_XXX_H
#define LIBCA_EM_UTIL_XXX_H

#include "../em_base/datatype.h"

// Type definitions
typedef struct xxx {
    // ... members
} xxx_t;

// Function declarations
void xxx_init(xxx_t* self, /* params */);
i32 xxx_function(xxx_t* self, /* params */);

#endif // LIBCA_EM_UTIL_XXX_H
```

**Guidelines**:
- Use `em_base/datatype.h` for standard types (u8, u16, u32, i32, etc.)
- Document complex parameters with `@brief` comments
- Keep the public API minimal and focused

#### 2.2 Source File (xxx.c)

Structure:

```c
#include "xxx.h"
#include "../em_base/debug.h"

/* ¡ª Implementation ¡ª */

void xxx_init(xxx_t* self, /* params */) {
    param_check(self != NULL);
    // ... implementation
}

i32 xxx_function(xxx_t* self, /* params */) {
    param_check(self != NULL);
    // ... implementation
    return XXX_OK;
}

/* ¡ª Unit Tests ¡ª */
#if TEST_ENABLE

#include "../em_test/test.h"

// Test cases go here

#endif
```

**Guidelines**:
- Use `param_check()` for parameter validation
- Use `debug_print()` for logging when helpful
- All error codes must be negative (if used)

### Step 3: Implement the Component

#### 3.1 Parameter Validation

Use `param_check()` macro from `em_base/debug.h`:

```c
#include "../em_base/debug.h"

i32 xxx_add(xxx_t* self, u32 value) {
    param_check(self != NULL);

    // ... implementation
    return XXX_OK;
}
```

#### 3.2 Error Handling

If functions return error codes:

```c
// Define error codes
#define XXX_OK                    0
#define XXX_ERR_INVALID_PARAM    (-1)
#define XXX_ERR_OVERFLOW        (-2)
#define XXX_ERR_BUFFER_FULL     (-3)

// Use them consistently
i32 xxx_write(xxx_t* self, u8 data) {
    if (xxx_is_full(self)) {
        return XXX_ERR_BUFFER_FULL;
    }
    // ... implementation
    return XXX_OK;
}
```

#### 3.3 Performance Considerations

- Avoid dynamic memory allocation in embedded contexts
- Prefer stack allocation or static buffers
- Minimize branching in hot paths
- Document time complexity for critical operations

### Step 4: Write Unit Tests

#### 4.1 Test Location and Structure

**CRITICAL**: All tests MUST be in the same `.c` file as the implementation, at the bottom:

```c
/* ¡ª Implementation ¡ª */
void my_function(/* params */) { ... }

/* ¡ª Unit Tests ¡ª */
#if TEST_ENABLE

#include "../em_test/test.h"

// Test cases here

#endif
```

#### 4.2 Atomic Testing Principle

**DO NOT** test everything in one `TEST_CASE`.
**DO** write one test case per feature/scenario.

**Bad example**:
```c
TEST_CASE(ringbuffer_all) {
    // Tests basic, wrap-around, overflow - ALL IN ONE ?
}
```

**Good example**:
```c
TEST_CASE(ringbuffer_basic) {
    // Tests basic write/read
}

TEST_CASE(ringbuffer_wrap_around) {
    // Tests wrap-around scenario
}

TEST_CASE(ringbuffer_overflow) {
    // Tests overflow handling
}
```

#### 4.3 Test Naming Convention

Format: `<module>_<feature>` or `<module>_<scenario>`

Examples:
- `ringbuffer_basic` - Basic functionality
- `ringbuffer_wrap_around` - Wrap-around scenario
- `ringbuffer_u16` - Specific data type
- `pid_position_basic` - Position PID basic test
- `crc_standard_vector` - Standard test vector

#### 4.4 Assertion Macros

Choose the right assertion for the data type:

```c
// Integers
TEST_ASSERT_EQUAL_INT(expected, actual);
TEST_ASSERT_EQUAL_UINT(expected, actual);

// Hexadecimal (good for bit operations)
TEST_ASSERT_EQUAL_HEX(expected, actual);

// Booleans
TEST_ASSERT_TRUE(condition);
TEST_ASSERT_FALSE(condition);

// Pointers
TEST_ASSERT_EQUAL_PTR(expected, actual);
TEST_ASSERT_NULL(pointer);
TEST_ASSERT_NOT_NULL(pointer);

// Strings
TEST_ASSERT_EQUAL_STRING(expected, actual);

// Memory blocks
TEST_ASSERT_EQUAL_MEMORY(expected, actual, size);

// Ranges
TEST_ASSERT_INT_WITHIN(min, max, actual);

// Floats (NEVER use == directly)
TEST_ASSERT_EQUAL_FLOAT(expected, actual);  // uses epsilon
```

#### 4.5 Required Test Coverage

For ALL components, test:

1. **Basic functionality**: Normal operation
2. **Boundary conditions**: 0, 1, MAX_SIZE
3. **Error handling**: If function can fail, test failure paths
4. **Standard vectors**: For algorithms like CRC, Hash, etc., include standard test vectors

**Example: RingBuffer tests**

```c
TEST_CASE(ringbuffer_basic) {
    u8 buf[16];
    ringbuffer_t rb;
    ringbuffer_init(&rb, buf, 16);

    TEST_ASSERT_EQUAL_INT(0, ringbuffer_used(&rb));

    u8 data = 0xAB;
    ringbuffer_write(&rb, &data, 1);
    TEST_ASSERT_EQUAL_INT(1, ringbuffer_used(&rb));
}

TEST_CASE(ringbuffer_boundary_zero) {
    u8 buf[16];
    ringbuffer_t rb;
    ringbuffer_init(&rb, buf, 16);

    TEST_ASSERT_EQUAL_INT(0, ringbuffer_read(&rb, &data, 1));
}

TEST_CASE(ringbuffer_wrap_around) {
    u8 buf[4];
    ringbuffer_t rb;
    ringbuffer_init(&rb, buf, 4);

    // Fill buffer
    u8 write_data[] = {0x01, 0x02, 0x03, 0x04};
    ringbuffer_write(&rb, write_data, 4);

    // Read all
    u8 read_data[4];
    ringbuffer_read(&rb, read_data, 4);

    // Write again (should wrap)
    ringbuffer_write(&rb, write_data, 4);
    TEST_ASSERT_EQUAL_INT(4, ringbuffer_used(&rb));
}
```

**Example: CRC algorithm tests**

```c
TEST_CASE(crc_standard_vector) {
    // Standard test vector: "123456789"
    u8 data[] = {0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37, 0x38, 0x39};
    u32 crc = crc32_calculate(data, 9);
    TEST_ASSERT_EQUAL_HEX(0xCBF43926, crc);  // Expected CRC-32 result
}

TEST_CASE(crc_boundary_empty) {
    u32 crc = crc32_calculate(NULL, 0);
    TEST_ASSERT_EQUAL_INT(0, crc);
}
```

#### 4.6 Test Helper Functions

For repeated test patterns, define helper functions:

```c
#if TEST_ENABLE

// Helper to fill buffer
static void fill_buffer(ringbuffer_t* rb, u8 value, usize size) {
    for (usize i = 0; i < size; i++) {
        u8 data = value;
        ringbuffer_write(rb, &data, 1);
    }
}

// Use in tests
TEST_CASE(ringbuffer_full) {
    u8 buf[8];
    ringbuffer_t rb;
    ringbuffer_init(&rb, buf, 8);

    fill_buffer(&rb, 0xAA, 8);
    TEST_ASSERT_EQUAL_INT(8, ringbuffer_used(&rb));
    TEST_ASSERT_TRUE(ringbuffer_is_full(&rb));
}

#endif
```

### Step 5: Integrate with Build System

#### 5.1 Add Library Target

Edit `src/em_util/xmake.lua`:

```lua
target("libca.em_util.xxx")
    set_kind("object")
    add_files("xxx.c")
    add_headerfiles("xxx.h")
```

#### 5.2 Add Test Target

In the same file (or in `src/em_test/xmake.lua`):

```lua
target("test-xxx")
    set_kind("binary")
    add_files("xxx.c")
    add_rules("em_test", {
        test_enable = true,
        use_default_main = true
    })
```

The `em_test` rule automatically:
- Defines `TEST_ENABLE=1`
- Links with test framework
- Provides `main()` function

#### 5.3 Run Tests

```bash
# Run all em/test tests
xmake test -g em/test

# Run specific test
xmake run test-xxx

# Run with verbose output
xmake test -g em/test -v
```

### Step 6: Verify Test Coverage

Check that all tests pass:

```bash
xmake test -g em/test -v
```

Ensure:
- ? All `TEST_CASE`s pass
- ? No memory leaks (use Valgrind or similar tools)
- ? Boundary conditions covered
- ? Error paths tested

## Common Component Patterns

### Algorithm Component (CRC)

**Header**:
```c
u32 crc32_calculate(const u8* data, u32 length);
```

**Tests**:
- Standard test vector ("123456789")
- Empty input (length = 0)
- NULL input (if supported)
- Random data

### Container Component (RingBuffer)

**Header**:
```c
typedef struct ringbuffer {
    u8* buffer;
    position_size_t size;
    position_size_t read;
    position_size_t write;
    position_size_t used;
} ringbuffer_t;

void ringbuffer_init(ringbuffer_t* rb, u8* buffer, position_size_t size);
position_size_t ringbuffer_write(ringbuffer_t* rb, const u8* data, position_size_t size);
position_size_t ringbuffer_read(ringbuffer_t* rb, u8* buffer, position_size_t size);
bool ringbuffer_is_full(ringbuffer_t* rb);
bool ringbuffer_is_empty(ringbuffer_t* rb);
```

**Tests**:
- Basic write/read
- Write one byte, read one byte
- Wrap-around (read catches up to write)
- Buffer full
- Buffer empty
- Boundary: size = 1, size = 0

### Filter Component (Moving Average)

**Header**:
```c
typedef struct moving_avg {
    u8* buffer;
    u8 index;
    u8 window_size;
    f32 sum;
    u8 count;
} moving_avg_t;

void moving_avg_init(moving_avg_t* filter, u8* buffer, u8 window_size);
f32 moving_avg_update(moving_avg_t* filter, f32 value);
```

**Tests**:
- Fill window completely
- Update before window is full
- Reset/overflow scenarios
- Numerical stability

### Utility Component (Bitmap)

**Header**:
```c
typedef struct bitmap {
    u8* data;
    usize size;  // in bits
} bitmap_t;

void bitmap_init(bitmap_t* bm, u8* buffer, usize size);
void bitmap_set(bitmap_t* bm, usize index);
void bitmap_clear(bitmap_t* bm, usize index);
bool bitmap_get(bitmap_t* bm, usize index);
```

**Tests**:
- Set and get bits
- Clear bits
- Boundary: first bit (index 0), last bit (index size-1)
- Out of bounds (index >= size)

## Coding Standards

### Type Usage

Use types from `em_base/datatype.h`:

```c
#include "../em_base/datatype.h"

// Unsigned integers
u8, u16, u32, u64  // Use for unsigned values

// Signed integers
i8, i16, i32, i64  // Use for signed values

// Floats
f32, f64           // Use for floating-point

// Size types
usize              // For sizes and indices
```

### Parameter Checking

Always use `param_check()` at the start of functions:

```c
#include "../em_base/debug.h"

void xxx_function(xxx_t* self, const u8* data, u32 length) {
    param_check(self != NULL);
    param_check(data != NULL);
    param_check(length > 0);

    // ... implementation
}
```

### Naming Conventions

- **Types**: `xxx_t` (snake_case with _t suffix)
- **Functions**: `xxx_function_name()` (snake_case)
- **Macros**: `XXX_DEFINE_MACRO` (UPPER_SNAKE_CASE)
- **Constants**: `XXX_CONSTANT` (UPPER_SNAKE_CASE)

## Resources

### scripts/

**create_component_skeleton.py**: Generate component code skeleton with test framework

```bash
python scripts/create_component_skeleton.py <component_name> <type>
```

Component types:
- `algorithm` - For algorithms (CRC, filters, etc.)
- `container` - For data structures (buffer, queue, etc.)
- `utility` - For utility functions

The script generates:
- `xxx.h` with type definitions and function declarations
- `xxx.c` with implementation and test framework structure
- Pre-configured test cases template

### references/

**µ¥Ôª²âÊÔ¹æ·¶.md**: Complete Chinese unit testing specification

Refer to this document for:
- Detailed testing guidelines
- Assertion macro reference
- Test best practices
- xmake integration details

**component_examples.md**: Analysis of existing components

This document contains:
- CRC algorithm implementation and tests
- RingBuffer container with comprehensive tests
- PID controller algorithm
- Bitmap utility
- Moving average filter

Each example includes:
- Complete source code
- Full test coverage
- Design rationale
- Common pitfalls

---

**Note**: Unlike em_driver, all em_util/em_base components MUST have comprehensive unit tests. Tests are automatically enabled during build via the `TEST_ENABLE` macro.
