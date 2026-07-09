# libca str design

`libca_str` provides text primitives above core. UTF-8 remains the general
internal text encoding, while the Java runtime foundation adds explicit UTF-16
types for Java-compatible string semantics.

## Main Components

- `Utf8String` and `Utf8StringRef`: owned and non-owned UTF-8 strings.
- `Utf8StringBuilder`, pools, arenas, and twine helpers: UTF-8 construction and
  storage utilities.
- `StringUtil`: byte-oriented string helpers such as ASCII classification,
  percent encoding, and base64url.
- `Char16`: a single UTF-16 code unit stored as `ca::u16`.
- `Utf16StringRef`: a non-owning UTF-16 view using Java code-unit indexing.
- `Utf16String`: an owned immutable UTF-16 string.
- `Utf16StringBuilder`: a mutable UTF-16 builder for append, insert, delete,
  reverse, and length adjustment.

## UTF-16 Semantics

UTF-16 APIs use code-unit indexes. `length()` returns the number of UTF-16 code
units, `char_at()` returns one code unit, and `code_point_at()` combines a valid
surrogate pair when the index points at the high surrogate. This matches the
parts of Java `String` and `StringBuilder` that a translator commonly needs.

## Runtime Boundary

The UTF-16 types do not model Java objects directly. They provide C++ storage
and algorithms. A translator-specific layer should adapt Java object layout,
GC/lifetime, null handling, and exception behavior.
