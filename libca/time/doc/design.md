# libca time design

`libca_time` provides simple time values and clock utilities. It depends on
core types and the C++ standard library, but does not depend on higher modules.

## Main Components

- `Duration`: a signed nanosecond interval value.
- `Timestamp`: a Unix-epoch nanosecond timestamp value.
- `Date`, `Time`, and `DateTime`: lightweight calendar display and parsing
  helpers.
- `TimeUtil`: clock helpers for runtime intrinsic mapping.

## Clock Semantics

`TimeUtil::current_time_millis()` returns Unix epoch milliseconds and is suitable
for Java `System.currentTimeMillis()` style mappings. `TimeUtil::nano_time()`
uses a monotonic clock and is only meaningful for relative elapsed-time
measurement, matching Java `System.nanoTime()` expectations.

## Runtime Boundary

`libca_time` does not implement Java `System` as a class. Translators should map
their intrinsic calls directly to the relevant `TimeUtil` function and keep
language-specific behavior outside libca.
