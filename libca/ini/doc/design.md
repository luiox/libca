# libca ini design

`libca_ini` is an independent INI configuration module. Its main goal is to let
callers read a human-edited file, update a few values, and write it back while
preserving comments, blank lines, and ordering.

## Main Components

- `IniDocument`: the editable, format-preserving document model.
- `IniReader`: parses strings or files into `IniDocument`.
- `IniWriter`: writes `IniDocument` back to strings or files.
- Record/index internals: preserve line order for output while maintaining
  section/key lookup indexes for queries.

## Design Notes

INI comments and blank lines are part of the document model. Unmodified lines
are written back as they were read. Modified key/value lines preserve useful
format pieces such as separators and inline comments where possible.

See `ini设计文档.md` and `ini使用文档.md` for the detailed preservation rules and
reader/writer usage.
