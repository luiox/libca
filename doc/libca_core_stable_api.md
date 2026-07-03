---
version: 2.0
update:
2026-06-18 - 精简为稳定性冻结清单；API 详情移至头文件 Doxygen 注释
2026-06-13 - 首版接口分级与冻结清单
---

# libca/core 稳定性冻结清单

> **本文只讲"哪些接口冻结了、稳定到什么程度"。具体 API 怎么用，查头文件的 Doxygen 注释。**
> 设计细节见 `libca/core/doc/{result-spec,bytes-spec,cast_design}.md`。

## 稳定性分级

| 模块 | 头文件 | 稳定性 | 说明 |
|---|---|---|---|
| 基础类型 | `datatype.hpp` | **Stable** | 全库类型入口，可固定 |
| 字节序列 | `bytes.hpp` | **Stable** | ByteSlice/Bytes/BytesMut，建议只增接口不改语义 |
| Result | `result.hpp` | **Stable** | `TRY` 宏仅 GCC/Clang（见头文件 @warning） |
| Any/type_id | `any.hpp` | **Stable** | `cast<T>` 不检查类型，优先 `as<T>` |
| 类型转换 | `cast.hpp` | **Stable** | 语义是**精确动态类型**匹配，非层级 |
| 导出宏 | `dllexport.hpp` | **Stable** | 仅 MSVC/Windows 语义 |
| 平台检测 | `platform.hpp` | Experimental | 引入系统头、仅 Win/Linux；勿在业务接口暴露 |
| 栈追踪 | `stacktrace.hpp` | Experimental | 输出格式不承诺，仅调试用 |
| 单例 | `wrapper.hpp` | Legacy | 不推广；新代码用 MeyersSingleton 或 DI |

稳定承诺：源码级兼容——下游可依赖类型名、函数名、参数、返回值和已写明的行为。**不**承诺二进制 ABI、对象内存布局、私有成员、异常/终止消息文本、内部扩容策略。

## 固定规则

- 公共命名空间以 `ca::core` 为主；`datatype.hpp` 的基础类型在 `ca`。
- `result.hpp` 保留 `namespace ca { using namespace ca::core; }` 兼容导出，新代码显式用 `ca::core`。
- 头文件按安装形式包含：`#include <libca/core/xxx.hpp>`。
- C++17。只把 public 类型/成员/非成员函数/公开宏列为稳定接口。

## 下游使用建议

```cpp
#include <libca/core/datatype.hpp>
#include <libca/core/result.hpp>
#include <libca/core/bytes.hpp>

ca::core::Result<ca::u32, std::string> parse_id(ca::core::Bytes bytes) {
    if (bytes.remaining() < 4) return ca::core::Err(std::string("short input"));
    return ca::core::Ok(bytes.get_u32_be());
}
```

优先用 `Result<T,E>` 表达可恢复错误、`Bytes`/`BytesMut` 表达字节协议、`dyn_cast<T>` 做精确类型分派、`Any` 仅用于确需类型擦除的边界。避免把 Experimental/Legacy 接口扩散到下游公共 API。
