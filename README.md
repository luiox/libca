# Cauptrue Artist

This is a C++ library for application development. Simple and easy to use.

Use C++17 and static link to use it.


# Features

## base

### Result

Result is the result of a function, which can be success or failure. It is used to return the result of a function.
And in library, it is used to return the result of a function. In the process of implementing the library, I have tried to use Results as much as possible and avoided using exceptions

### Charset

This provides conversion of character encoding and mutual conversion between various miscellaneous strings in the C++ standard library.

### String

Because of C++'s `std::string` does not provide encoding, so an enhanced implementation of String with encoding is provided.

### ByteBuffer

ByteBuffer is a byte buffer that can be used to store and manipulate binary data. It is similar to `std::vector<uint8_t>`, but with additional functionality for reading and writing data.

### Format

Format is a tool that provides a way to format strings in a similar way to format string like `xxx {}`.


# How to run

test one target, xmake will build and run it.

```shell
xmake run target_name
```

if you want to run all test.
```shell
xmake test -g em/test
```

if you want to watch the output detail, add `-v` argument.
```shell
xmake test -g em/test -v
```


