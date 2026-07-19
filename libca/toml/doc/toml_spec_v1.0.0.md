# TOML 1.0.0 规范（存档）

> 来源：https://toml.io/en/v1.0.0 （Published 2021-01-11）
> 存档目的：实现 `libca/toml` 的权威语法依据，避免每次联网。
> 实现向摘要见 `dev_plan.md` 第 2 章；本文是规范原文。

Tom's Obvious, Minimal Language. By Tom Preston-Werner, Pradyun Gedam, et al.

## Objectives

TOML aims to be a minimal configuration file format that's easy to read due to
obvious semantics. TOML is designed to map unambiguously to a hash table. TOML
should be easy to parse into data structures in a wide variety of languages.

## Spec

- TOML is case-sensitive.
- A TOML file must be a valid UTF-8 encoded Unicode document.
- Whitespace means tab (0x09) or space (0x20).
- Newline means LF (0x0A) or CRLF (0x0D 0x0A).

A hash symbol marks the rest of the line as a comment, except when inside a
string.

```
# This is a full-line comment
key = "value"  # This is a comment at the end of a line
another = "# This is not a comment"
```

Control characters other than tab (U+0000 to U+0008, U+000A to U+001F, U+007F)
are not permitted in comments.

## Key/Value Pair

The primary building block of a TOML document is the key/value pair.

Keys are on the left of the equals sign and values are on the right. Whitespace
is ignored around key names and values. The key, equals sign, and value must be
on the same line (though some values can be broken over multiple lines).

```
key = "value"
```

Values must have one of the following types: String, Integer, Float, Boolean,
Offset Date-Time, Local Date-Time, Local Date, Local Time, Array, Inline Table.

Unspecified values are invalid. `key = # INVALID`. There must be a newline (or
EOF) after a key/value pair (see Inline Table for exceptions).

## Keys

A key may be either bare, quoted, or dotted.

**Bare keys** may only contain ASCII letters, ASCII digits, underscores, and
dashes (`A-Za-z0-9_-`). Bare keys may be composed of only ASCII digits (e.g.
`1234`), but are always interpreted as strings.

**Quoted keys** follow the exact same rules as either basic strings or literal
strings and allow a much broader set of key names. Best practice is to use bare
keys except when absolutely necessary.

A bare key must be non-empty, but an empty quoted key is allowed (though
discouraged).

**Dotted keys** are a sequence of bare or quoted keys joined with a dot. This
allows for grouping similar properties together:

```
name = "Orange"
physical.color = "orange"
physical.shape = "round"
site."google.com" = true
```

JSON equivalent:

```json
{
  "name": "Orange",
  "physical": { "color": "orange", "shape": "round" },
  "site": { "google.com": true }
}
```

Whitespace around dot-separated parts is ignored. Indentation is treated as
whitespace and ignored.

Defining a key multiple times is invalid. Bare keys and quoted keys are
equivalent (`spelling = "favorite"` then `"spelling" = "favourite"` is invalid).

As long as a key hasn't been directly defined, you may still write to it and to
names within it:

```
# This makes the key "fruit" into a table.
fruit.apple.smooth = true
# So then you can add to the table "fruit" like so:
fruit.orange = 2
```

```
# INVALID: turns an integer into a table
fruit.apple = 1
fruit.apple.smooth = true
```

Defining dotted keys out-of-order is discouraged but valid.

Since bare keys can be composed of only ASCII integers, dotted keys may look
like floats (`3.14159 = "pi"` → `{"3": {"14159": "pi"}}`).

## String

Four ways: basic, multi-line basic, literal, multi-line literal. All strings
must contain only valid UTF-8 characters.

**Basic strings** are surrounded by quotation marks (`"`). Any Unicode may be
used except those that must be escaped: quotation mark, backslash, and control
characters other than tab (U+0000-U+0008, U+000A-U+001F, U+007F).

```
str = "I'm a string. \"You can quote me\". Name\tJos\u00E9\nLocation\tSF."
```

Compact escape sequences:

```
\b         - backspace       (U+0008)
\t         - tab             (U+0009)
\n         - linefeed        (U+000A)
\f         - form feed       (U+000C)
\r         - carriage return (U+000D)
\"         - quote           (U+0022)
\\         - backslash       (U+005C)
\uXXXX     - unicode         (U+XXXX)
\UXXXXXXXX - unicode         (U+XXXXXXXX)
```

Escape codes must be valid Unicode scalar values. All other escape sequences are
reserved and must produce an error.

**Multi-line basic strings** are surrounded by three quotation marks on each
side and allow newlines. A newline immediately following the opening delimiter
will be trimmed. All other whitespace and newline characters remain intact.

```
str1 = """
Roses are red
Violets are blue"""
```

TOML parsers may normalize newline to whatever makes sense for their platform.

For writing long strings without extraneous whitespace, use a "line ending
backslash". When the last non-whitespace character on a line is an unescaped
`\`, it will be trimmed along with all whitespace (including newlines) up to the
next non-whitespace character or closing delimiter. All escape sequences valid
for basic strings are also valid for multi-line basic strings.

```
# These are byte-for-byte equivalent:
str1 = "The quick brown fox jumps over the lazy dog."
str2 = """
The quick brown \

  fox jumps over \
    the lazy dog."""
str3 = """\
       The quick brown \
       fox jumps over \
       the lazy dog.\
       """
```

Any Unicode character may be used except those that must be escaped: backslash
and control characters other than tab, line feed, and carriage return
(U+0000-U+0008, U+000B, U+000C, U+000E-U+001F, U+007F).

You can write a quotation mark, or two adjacent quotation marks, anywhere inside
a multi-line basic string, including just inside the delimiters. (Three adjacent
requires escaping the middle one.)

**Literal strings** are surrounded by single quotes. Like basic strings, they
must appear on a single line. No escaping whatsoever:

```
winpath  = 'C:\Users\nodejs\templates'
winpath2 = '\\ServerX\admin$\system32\'
quoted   = 'Tom "Dubs" Preston-Werner'
regex    = '<\i\c*\s*>'
```

Since there's no escaping, you can't write a single quote inside a single-line
literal string. Multi-line literal strings solve this.

**Multi-line literal strings** are surrounded by three single quotes on each
side and allow newlines. No escaping. A newline immediately following the
opening delimiter will be trimmed. All other content between delimiters is
interpreted as-is.

```
regex2 = '''I [dw]on't need \d{2} apples'''
lines  = '''
The first newline is
trimmed in raw strings.
   All other whitespace
   is preserved.
'''
```

You can write 1 or 2 single quotes anywhere within a multi-line literal string,
but sequences of three or more single quotes are not permitted.

Control characters other than tab are not permitted in a literal string. For
binary data, use Base64 or another suitable ASCII/UTF-8 encoding.

## Integer

Integers are whole numbers. Positive may be prefixed with `+`, negative with `-`.

```
int1 = +99
int2 = 42
int3 = 0
int4 = -17
```

For large numbers, underscores between digits enhance readability. Each
underscore must be surrounded by at least one digit on each side.

```
int5 = 1_000
int6 = 5_349_221
int7 = 53_49_221  # Indian number system grouping
int8 = 1_2_3_4_5  # VALID but discouraged
```

Leading zeros are not allowed. `-0` and `+0` are valid and identical to `0`.

Non-negative integers may also be expressed in hex/octal/binary. In these
formats, leading `+` is not allowed and leading zeros are allowed (after the
prefix). Hex is case-insensitive. Underscores allowed between digits (not
between prefix and value).

```
# hexadecimal with prefix 0x
hex1 = 0xDEADBEEF
hex2 = 0xdeadbeef
hex3 = 0xdead_beef

# octal with prefix 0o
oct1 = 0o01234567
oct2 = 0o755 # useful for Unix file permissions

# binary with prefix 0b
bin1 = 0b11010110
```

Arbitrary 64-bit signed integers (from −2^63 to 2^63−1) should be accepted and
handled losslessly. If an integer cannot be represented losslessly, an error
must be thrown.

## Float

Floats should be implemented as IEEE 754 binary64 values.

A float consists of an integer part (same rules as decimal integer) followed by
a fractional part and/or an exponent part. If both present, fractional precedes
exponent.

```
# fractional
flt1 = +1.0
flt2 = 3.1415
flt3 = -0.01

# exponent
flt4 = 5e+22
flt5 = 1e06
flt6 = -2E-2

# both
flt7 = 6.626e-34
```

A fractional part is a decimal point followed by one or more digits. An exponent
part is E (upper or lower) followed by an integer part (may include leading
zeros). The decimal point must be surrounded by at least one digit on each side.

```
# INVALID FLOATS
invalid_float_1 = .7
invalid_float_2 = 7.
invalid_float_3 = 3.e+20
```

Underscores may be used, each surrounded by at least one digit:
`flt8 = 224_617.445_991_228`.

`-0.0` and `+0.0` are valid and map per IEEE 754.

Special float values (always lowercase):

```
# infinity
sf1 = inf  # positive infinity
sf2 = +inf # positive infinity
sf3 = -inf # negative infinity

# not a number
sf4 = nan  # actual sNaN/qNaN encoding is implementation-specific
sf5 = +nan # same as nan
sf6 = -nan # valid, actual encoding is implementation-specific
```

## Boolean

```
bool1 = true
bool2 = false
```

Always lowercase.

## Offset Date-Time

RFC 3339 formatted date-time with offset:

```
odt1 = 1979-05-27T07:32:00Z
odt2 = 1979-05-27T00:32:00-07:00
odt3 = 1979-05-27T00:32:00.999999-07:00
```

For readability, you may replace the T delimiter with a space character (per RFC
3339 section 5.6): `odt4 = 1979-05-27 07:32:00Z`.

Millisecond precision is required. Further precision of fractional seconds is
implementation-specific. If the value contains greater precision than the
implementation can support, the additional precision must be truncated, not
rounded.

## Local Date-Time

Omit the offset: represents the date-time without any relation to an offset or
timezone.

```
ldt1 = 1979-05-27T07:32:00
ldt2 = 1979-05-27T00:32:00.999999
```

Millisecond precision required; further precision implementation-specific;
excess truncated, not rounded.

## Local Date

Only the date portion:

```
ld1 = 1979-05-27
```

## Local Time

Only the time portion:

```
lt1 = 07:32:00
lt2 = 00:32:00.999999
```

Millisecond precision required; further precision implementation-specific;
excess truncated, not rounded.

## Array

Square brackets with values inside. Whitespace ignored. Elements separated by
commas. Arrays can contain values of the same types as allowed in key/value
pairs. Values of different types may be mixed.

```
integers = [ 1, 2, 3 ]
colors = [ "red", "yellow", "green" ]
nested_arrays_of_ints = [ [ 1, 2 ], [3, 4, 5] ]
nested_mixed_array = [ [ 1, 2 ], ["a", "b", "c"] ]
string_array = [ "all", 'strings', """are the same""", '''type''' ]

# Mixed-type arrays allowed
numbers = [ 0.1, 0.2, 0.5, 1, 2, 5 ]
contributors = [
  "Foo Bar <foo@example.com>",
  { name = "Baz Qux", email = "bazqux@example.com>", url = "https://example.com/bazqux" }
]
```

Arrays can span multiple lines. A terminating comma (trailing comma) is permitted
after the last value. Any number of newlines and comments may precede values,
commas, and the closing bracket.

```
integers2 = [
  1, 2, 3
]

integers3 = [
  1,
  2, # this is ok
]
```

## Table

Tables are defined by headers with square brackets on a line by themselves.
Arrays are distinguished from headers because arrays are only ever values.

```
[table]
```

Under that, until the next header or EOF, are the key/values of that table.

```
[table-1]
key1 = "some string"
key2 = 123

[table-2]
key1 = "another string"
key2 = 456
```

Naming rules for tables are the same as for keys.

```
[dog."tater.man"]
type.name = "pug"
```

JSON: `{ "dog": { "tater.man": { "type": { "name": "pug" } } } }`.

Whitespace around keys is ignored. Indentation is treated as whitespace.

```
[a.b.c]            # best practice
[ d.e.f ]          # same as [d.e.f]
[ g .  h  . i ]    # same as [g.h.i]
[ j . "ʞ" . 'l' ]  # same as [j."ʞ".'l']
```

You don't need to specify all the super-tables:

```
# [x] you
# [x.y] don't
# [x.y.z] need these
[x.y.z.w] # for this to work

[x] # defining a super-table afterward is ok
```

Empty tables are allowed (no key/value pairs).

You cannot define a table more than once:

```
# DO NOT DO THIS
[fruit]
apple = "red"
[fruit]
orange = "orange"

# DO NOT DO THIS EITHER
[fruit]
apple = "red"
[fruit.apple]
texture = "smooth"
```

Defining tables out-of-order is discouraged but valid.

The top-level (root) table starts at the beginning of the document and ends just
before the first table header (or EOF). It is nameless.

Dotted keys create and define a table for each key part before the last one,
provided such tables were not previously created:

```
fruit.apple.color = "red"
# Defines tables "fruit" and "fruit.apple"

fruit.apple.taste.sweet = true
# Defines table "fruit.apple.taste"; fruit and fruit.apple already created
```

Since tables cannot be defined more than once, redefining such tables using a
`[table]` header is not allowed. Likewise using dotted keys to redefine tables
already defined in `[table]` form. The `[table]` form can define sub-tables
within tables defined via dotted keys:

```
[fruit]
apple.color = "red"
apple.taste.sweet = true

# [fruit.apple]  # INVALID
# [fruit.apple.taste]  # INVALID

[fruit.apple.texture]  # you can add sub-tables
smooth = true
```

## Inline Table

Compact syntax for tables, fully defined within `{` and `}`. Zero or more
comma-separated key/value pairs. Value types allowed include inline tables.

Inline tables are intended to appear on a single line. A terminating comma is
**not** permitted after the last key/value pair. No newlines allowed between the
braces unless valid within a value. Strongly discouraged to break across lines.

```
name = { first = "Tom", last = "Preston-Werner" }
point = { x = 1, y = 2 }
animal = { type.name = "pug" }
```

Equivalent to standard tables.

Inline tables are fully self-contained and define all keys and sub-tables within
them. Keys and sub-tables cannot be added outside the braces:

```
[product]
type = { name = "Nail" }
# type.edible = false  # INVALID
```

Similarly, inline tables cannot add keys or sub-tables to an already-defined
table:

```
[product]
type.name = "Nail"
# type = { edible = false }  # INVALID
```

## Array of Tables

Expressed by a header with a name in double brackets. The first instance of that
header defines the array and its first table element, and each subsequent
instance creates and defines a new table element. Tables inserted in order
encountered.

```
[[products]]
name = "Hammer"
sku = 738594937

[[products]]  # empty table within the array

[[products]]
name = "Nail"
sku = 284758393

color = "gray"
```

JSON:

```json
{
  "products": [
    { "name": "Hammer", "sku": 738594937 },
    { },
    { "name": "Nail", "sku": 284758393, "color": "gray" }
  ]
}
```

Any reference to an array of tables points to the **most recently defined table
element** of the array. This allows defining sub-tables, and even sub-arrays of
tables, inside the most recent table:

```
[[fruits]]
name = "apple"

[fruits.physical]  # subtable of most recent [[fruits]]
color = "red"
shape = "round"

[[fruits.varieties]]  # nested array of tables
name = "red delicious"

[[fruits.varieties]]
name = "granny smith"

[[fruits]]
name = "banana"

[[fruits.varieties]]
name = "plantain"
```

JSON:

```json
{
  "fruits": [
    {
      "name": "apple",
      "physical": { "color": "red", "shape": "round" },
      "varieties": [
        { "name": "red delicious" },
        { "name": "granny smith" }
      ]
    },
    {
      "name": "banana",
      "varieties": [ { "name": "plantain" } ]
    }
  ]
}
```

If the parent of a table or array of tables is an array element, that element
must already have been defined before the child can be defined. Reversing that
ordering must produce an error at parse time:

```
# INVALID TOML DOC
[fruit.physical]  # subtable, but to which parent element should it belong?
color = "red"
shape = "round"

[[fruit]]  # parser must throw an error: "fruit" is an array, not a table
name = "apple"
```

Attempting to append to a statically defined array, even empty, must error:

```
# INVALID TOML DOC
fruits = []

[[fruits]] # Not allowed
```

Attempting to define a normal table with the same name as an already established
array must error. Attempting to redefine a normal table as an array must error:

```
# INVALID TOML DOC
[[fruits]]
name = "apple"

[fruits.varieties]      # conflicts with the array of tables below
name = "red delicious"
```

Inline tables may also be used:

```
points = [ { x = 1, y = 2, z = 3 },
           { x = 7, y = 8, z = 9 },
           { x = 2, y = 4, z = 8 } ]
```

## Filename Extension

TOML files should use the extension `.toml`.

## MIME Type

`application/toml`.

## ABNF Grammar

A formal description of TOML's syntax is available as a separate ABNF file (see
https://github.com/toml-lang/toml/blob/1.0.0/toml.abnf ).
