# AGENTS Rules

## em_driver Header Manual Rules

- Do not use scripts to modify code. Forbidden: batch shell/perl/python/sed/awk auto-edits.
- All driver header updates under `src/em_driver` must be edited manually, file by file.
- For C++ compatibility, every driver header must include:
  - `#ifdef __cplusplus`
  - `extern "C" {`
  - matching closing guard before final header `#endif`.
- Keep header changelog note in file header comment:
  - `* @update 0.2 添加extern外部依赖注入模式`
- Doxygen style requirement:
  - Do not use one-line blocks that compress `@brief/@param/@return` into one line.
  - Use multi-line Doxygen blocks.
- Before finalizing, re-read modified headers and check all above rules again.

## Self-Reminder

- If any requirement is forgotten, stop and read this file again before continuing.

- After applying changes, build the project and run all related tests. Iteratively fix any compilation errors or test failures until the build is clean and all tests pass.
