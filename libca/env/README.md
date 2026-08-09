# libca_env

环境变量与操作系统信息查询。命名空间 `ca::env`，构建目标 `libca_env`。

对标 Rust `std::env` 的轻量封装。仅桌面端使用（嵌入式不需要）。Windows 上内部做
UTF-8 ↔ UTF-16 转换，保证含非 ASCII 的环境变量正确往返。所有函数不抛异常。

## 用法

```cpp
#include <libca/env/env.hpp>

using namespace ca::env;

auto path = get("PATH");                  // std::optional<std::string>
set("MY_TOOL_DEBUG", "1");
remove("MY_TOOL_DEBUG");

for (const auto& [key, value] : all())    // 全部环境变量
    process(key, value);

std::string cwd = current_dir();
set_current_dir("/tmp");

std::string tmp = temp_dir();
std::string exe = executable_path();
std::string os  = os_name();              // "windows" / "linux" / "macos"
std::string ver = os_version();           // "10.0.22631"
```

## 接口

| 函数 | 说明 |
|------|------|
| `get(name)` | 读取环境变量，不存在返回空 optional |
| `set(name, value)` | 设置（覆盖）环境变量 |
| `remove(name)` | 删除；变量本就不存在也返回 true |
| `all()` | 所有 key=value 对（驱动器当前目录条目已过滤） |
| `current_dir()` | 当前工作目录（UTF-8） |
| `set_current_dir(path)` | 切换工作目录 |
| `temp_dir()` | 临时目录（末尾无分隔符） |
| `executable_path()` | 当前可执行文件绝对路径 |
| `os_name()` | `"windows"` / `"linux"` / `"macos"` / `"unknown"` |
| `os_version()` | 版本字符串（如 `10.0.22631`） |

## 平台实现

- **Windows**：统一用 `*W` 宽字符 API（`GetEnvironmentVariableW`/`SetEnvironmentVariableW`/
  `GetEnvironmentStringsW`/`GetModuleFileNameW`/`GetTempPathW`），经 `MultiByteToWideChar`/
  `WideCharToMultiByte` 做 UTF-8 ↔ UTF-16。`os_version` 用 `RtlGetVersion`（动态加载 ntdll），
  比 `GetVersionEx` 更可靠（不受 manifest 兼容性声明影响）。
- **Linux**：`getenv`/`setenv`/`unsetenv`/`environ`/`getcwd`；`executable_path` 读
  `/proc/self/exe`；`os_version` 解析 `/etc/os-release`。
- **macOS**：`_NSGetExecutablePath` 取可执行路径；`os_version` 调 `sw_vers -productVersion`。

## 依赖

`libca_core`。Windows 上 UTF-8 ↔ UTF-16 转换直接用 Win32 API，不依赖 str 模块，保持系统层
不跨层依赖基础字符串层。
