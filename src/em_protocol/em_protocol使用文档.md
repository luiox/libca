# em_protocol 使用文档

## 快速接入

```lua
add_moduledirs(path.join(os.scriptdir(), "..", "xmake", "modules"))

target("app")
    set_kind("binary")
    add_files("app/main.c")
    on_load(function (target)
        local em = import("libca.em")
        em.setup(target, {root = path.join(os.scriptdir(), "..")})
        em.add_libs(target, "em_protocol")
    end)
```

`em_protocol` 会自动补齐 `em_base` 与 `em_util`。

## 头文件示例

```c
#include <em_protocol/transport.h>
#include <em_protocol/xmodem.h>
#include <em_protocol/ymodem.h>
#include <em_protocol/file_transfer.h>
```

## 使用建议

1. 先实现 `transport` 读写适配层，再接协议状态机。
2. 在业务层只关心回调与数据处理，不直接耦合底层串口实现。
3. 需要仿真时可参考目录内 `test-*.c` 的组织方式。

## 常见问题

1. 协议无响应：优先检查超时参数和底层 `read/write` 回调返回值。
2. 解析异常：检查输入缓冲是否被多线程并发写入。
