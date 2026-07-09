# libca collection design

`libca_collection` provides small, stable collection wrappers and collection
helpers. It is a thin layer over standard storage types, but exposes names and
methods that are easier for generated runtime code to target.

## Main Components

- `ArrayList<T>`: an owned contiguous list backed by `std::vector<T>`.
- `HashMap<K, V>`: an owned hash map backed by `std::unordered_map`.
- `HashSet<T>`: an owned hash set backed by `std::unordered_set`.
- `ImmutableList<T>`: a simple immutable list for configuration-like data.
- `Stream`: a lightweight lazy processing helper.

## API Style

Collection type names are PascalCase (`ArrayList`, `HashMap`, `HashSet`) while
methods use snake_case (`len`, `is_empty`, `get_or_insert`, `swap_remove`).
This keeps generated code readable without copying Java method names directly.

APIs that may miss return pointers or `std::optional`; APIs that violate
container bounds throw standard C++ exceptions. Hash containers do not guarantee
iteration order.

## Runtime Boundary

These containers are not Java collections. They provide C++ data structures that
a translator runtime can wrap. Java object identity, generics erasure, GC, and
exception mapping must stay outside this module.
