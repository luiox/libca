# DynamicLibrary Design

## Goal

`DynamicLibrary` provides the small cross-platform runtime loading primitive
needed by plugin hosts. It supports loading a library from a UTF-8 path,
finding an exported symbol, and explicitly or automatically releasing the
native handle.

The implementation supports Windows and Linux, which are the platforms
supported by `libca/core/platform.hpp`.

## Public API

```cpp
class DynamicLibrary {
public:
    static StatusResult<DynamicLibrary> load(const std::string& path);

    template<typename T>
    StatusResult<T*> lookup(const std::string& symbol_name) const;

    void unload() noexcept;
    bool is_loaded() const noexcept;
};
```

The class is move-only. Destruction calls `unload()`, so an acquired native
handle always has one owner. A moved-from instance is empty and may safely be
unloaded or destroyed.

`lookup<T>` requires `T` to be a function type and returns a pointer to that
function. The returned pointer is owned by the dynamic library and must not be
called after `unload()` or destruction.

## Error Model

Fallible operations return `StatusResult<T>`, using existing `Status` types:

- `NOT_FOUND`: the library file or requested symbol was not found.
- `FAILED_PRECONDITION`: `lookup()` was called after unload or on a moved-from
  object.
- `INVALID_ARGUMENT`: the supplied path or symbol name was empty.
- `INTERNAL`: a platform loader error not covered by the categories above.

The status message includes the path or symbol and the platform diagnostic
where available.

## Platform Mapping

| Operation | Windows | Linux |
| --- | --- | --- |
| Load | `LoadLibraryW` after UTF-8 to UTF-16 conversion | `dlopen(path, RTLD_NOW | RTLD_LOCAL)` |
| Lookup | `GetProcAddress` | `dlsym` |
| Unload | `FreeLibrary` | `dlclose` |

On Linux the module links `dl`; Windows uses `LoadLibraryW` as requested.

## Concurrency and Lifetime

Separate `DynamicLibrary` instances may be used on different threads. A single
instance must not be used concurrently with `unload()` and `lookup()`, because
the loader may release a symbol while another thread is using it. Plugin hosts
must keep the instance alive for the whole period in which they invoke its
symbols.

## Test Strategy

Platform tests load a known system library, resolve `getpid` on Linux or
`GetCurrentProcessId` on Windows, and call it. Additional tests cover a missing
file, a missing symbol, and rejection of lookup after unload.
