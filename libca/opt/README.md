# libca_opt

命令行选项解析器。命名空间 `ca::opt`，构建目标 `libca_opt`。

定位对标 Rust `clap`/`argh` 或 Python `argparse`，保持 C++ 风格不做过度设计。覆盖常见
CLI 形态：短名/长名、`--name value`/`--name=value`、required、子命令嵌套、`--help`、`--`。

## 用法

```cpp
#include <libca/opt/opt.hpp>
#include <iostream>

using ca::opt::Arg;
using ca::opt::Command;
using ca::opt::Parser;

int main(int argc, const char* const argv[]) {
    Command root;
    root.name = "mytool";
    root.help = "do useful things";
    Arg verbose;  verbose.name = "verbose"; verbose.short_name = 'v'; verbose.help = "verbose";
    Arg output;   output.name   = "output";  output.short_name   = 'o';
                  output.has_value = true;   output.help = "output file"; output.required = true;
    root.args = {verbose, output};

    Parser parser(root);
    auto result = parser.parse(argc, argv);
    if (result.is_err()) {
        auto st = result.unwrap_err();
        std::cerr << st.message() << "\n";
        return st.code() == ca::core::StatusCode::CANCELLED ? 0 : 1;  // help 退出码 0
    }

    auto parsed = result.unwrap();
    if (parsed.has("verbose")) enable_verbose();
    open_output(parsed.get("output"));
    return 0;
}
```

## 支持的形态

| 形态 | 示例 | 说明 |
|------|------|------|
| 长开关 | `--verbose` | 布尔 flag，出现即 true |
| 短开关 | `-v` | 同上 |
| 组合短开关 | `-abg` | 多个布尔 flag 合并 |
| 长选项带值 | `--output FILE` / `--output=FILE` | 两种等价 |
| 短选项带值 | `-o FILE` / `-oFILE` | 两种等价 |
| 默认值 | `--count 1`（缺省） | `Arg.default_value` |
| 必填校验 | 缺失返回错误 | `Arg.required` |
| 子命令 | `git commit -m msg` | 按首个非选项 token 分派，可嵌套 |
| 终止符 | `a -- --not-an-option` | `--` 后全部当位置参数（当前不暴露位置参数访问，仅用于截断选项解析） |
| 帮助 | `--help` / `-h` | 任意层级可用，返回 CANCELLED + 帮助文本 |

## `Arg` 约束

- `name` 必须非空。它是 `has()`/`get()` 取值的唯一 key；即便选项只用短名（如 `-v`），
  也要提供一个长名作为存储 key。重复长名或短名会在 `parse` 时返回 `INVALID_ARGUMENT`。
- `required` 与 `default_value` 互斥：有默认值意味着 `has()` 恒为真，required 校验永远
  不触发，`parse` 会拒绝这种组合。

## `--help` 的特殊返回

`parse()` 返回 `StatusResult<ParseResult>`。`--help` 不产出 ParseResult，走 `Err` 分支，
但 **status code 为 `CANCELLED`**（区别于真正的解析错误 `INVALID_ARGUMENT`）。调用方：

```cpp
if (result.is_err()) {
    auto st = result.unwrap_err();
    std::cerr << st.message() << "\n";           // help 文本或错误信息
    return st.code() == StatusCode::CANCELLED ? 0 : 1;
}
```

> 不用 `OK` code 是因为 `Status::error(OK, ...)` 会把 OK 归一化为 UNKNOWN，无法区分。
> `CANCELLED` 语义上接近"用户主动中止正常流程"，最贴近 help 的含义。

## 依赖

`libca_core`（用 `Result`/`Status`/`StatusCode`）。
