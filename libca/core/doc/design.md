# libca core design

`libca_core` is the lowest C++ layer in libca. Other C++ modules may depend on
it, but core must not depend on higher modules such as str, fs, time, crypto, or
collection.

## Main Components

- `datatype.hpp`: fixed-width and semantic aliases such as `u8`, `i32`, and
  `usize`.
- `Result<T, E>` and `Status`: explicit success/error values for APIs that
  should not use exceptions as ordinary control flow.
- `Bytes`, `BytesMut`, and `ByteSlice`: byte buffers and views with explicit
  ownership and endian-aware access.
- `ScopeGuard` and `DEFER`: RAII cleanup helpers.
- `MathUtil`: small stateless math helpers used by runtime intrinsic mapping.
- `ArrayUtil`: raw-array helpers for equality, fill, and copy operations.

## API Rules

Core APIs should be small, stable, and dependency-light. Public headers carry
Doxygen comments for usage, ownership, return values, and exceptional cases.
Design documents explain module boundaries; they should not replace API
comments.

## Runtime Boundary

The Java translator runtime may map common intrinsics onto `MathUtil` and
`ArrayUtil`, but Java object layout, GC/lifetime rules, and Java exception
policy belong in the translator-side runtime wrapper, not in `libca_core`.
