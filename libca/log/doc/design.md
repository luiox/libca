# libca log design

`libca_log` is a lightweight logging facade. It separates logging call sites
from concrete backends and keeps disabled log paths cheap.

## Main Components

- `Level`: log severity enum.
- `ILogBackend`: backend interface implemented by adapters.
- `Logger`: runtime filtering facade over a backend.
- `SpdlogBackend`: adapter to spdlog.
- `LOG_*` and `LOGT_*` macros: call-site helpers with compile-time and runtime
  filtering.

## Design Notes

Compile-time filtering removes calls below `COMPILE_LOG_LEVEL`. Runtime filtering
checks an atomic backend level before formatting, so disabled log messages avoid
formatting cost. The global logger uses shared ownership for lifetime and an
atomic raw pointer for fast reads.

See `log.md` for setup examples and detailed macro usage.
