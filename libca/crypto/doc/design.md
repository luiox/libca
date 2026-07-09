# libca crypto design

`libca_crypto` provides basic encoding, checksum, digest, HMAC, random, and
lightweight stream-cipher primitives. It is a primitives module, not a complete
security protocol library.

## Main Components

- Encoding: `base64.hpp` and `hex.hpp`.
- Checksums: `crc.hpp`.
- Hashes: `md5.hpp`, `sha1.hpp`, `sha256.hpp`, and `sha3.h`.
- Authentication: `hmac.hpp`.
- Random bytes: `random.hpp`.
- Stream ciphers: `rc4.hpp` and `chacha20.hpp`.
- Errors and helpers: `crypto_error.hpp`, `crypto_util.hpp`, and `crypto.hpp`.

## Design Notes

Newer APIs prefer `ByteSlice` input and `Bytes` or `std::string` output so
callers can pass arrays, strings, or libca byte buffers through one view type.
Format errors use `Result<T, CryptoError>` where the caller needs a reason.

See `crypto设计文档.md` for algorithm boundaries, security notes, and suggested
reading order.
