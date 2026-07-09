# libca fs design

`libca_fs` wraps C++17 `std::filesystem` with libca naming and error-model
conventions. It provides path string helpers and file-system operations without
pulling policy into callers.

## Main Components

- `PathUtil`: pure path-string operations such as normalize, join, extension,
  stem, filename, parent, absolute checks, and split.
- `FileUtil`: file and directory IO helpers, including read/write, atomic
  write, metadata, permissions, traversal, copy/move/remove, temporary files,
  and backup.
- `FsError`: module-level error codes used by fallible file-system operations.

## Design Notes

`PathUtil` does not touch the file system. `FileUtil` handles side effects and
converts most recoverable failures to `Result<T, FsError>` or simple boolean
results. Atomic writes use a same-directory temporary file and never delete the
old target when the final rename fails.

See `fs设计文档.md` for the detailed behavior of atomic write, directory copy,
glob traversal, and symlink handling.
