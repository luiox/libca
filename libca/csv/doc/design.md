# libca csv design

`libca_csv` is an independent CSV parser and writer module. It replaces the old
utility CSV helper with a model/reader/writer split and better quoted-field
handling.

## Main Components

- `CsvRow`: one record of fields.
- `CsvDocument`: an optional header plus data rows.
- `CsvReader`: parses strings or files into `CsvDocument`.
- `CsvWriter`: serializes `CsvDocument` to strings or files.
- `CsvReaderOptions` and `CsvWriterOptions`: format and compatibility switches.

## Design Notes

The document model stores field values rather than original separators, quotes,
or line endings. Formatting decisions live in reader/writer options. Reader and
writer functions use `Result<T, std::string>` where file IO or malformed input
can fail.

See `csv设计文档.md` and `csv使用文档.md` for detailed parsing and writing
behavior.
