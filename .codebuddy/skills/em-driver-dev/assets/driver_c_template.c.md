# Driver C 模板

```c
#include "xxx.h"
#include "../em_base/debug.h"
#include "../em_base/datatype.h"

static const xxx_port_t* g_xxx_port = NULL;

void xxx_bind_port(const xxx_port_t* port) {
    g_xxx_port = port;
}

bool xxx_port_is_registered(void) {
    return g_xxx_port != NULL;
}

#define XXX_WRITE(self, v)    g_xxx_port->write((self)->gpio, (self)->pin, (v))

i32 xxx_do_something(xxx_t* self) {
    param_check(self != NULL);
    param_check(g_xxx_port != NULL);

    // 实现细节
    return 0; // XXX_OK
}

// 私有函数用 static 并加注释
static void helper_process(void) {
    // ...
}
```

说明：使用 `param_check` 对关键的 self 指针做契约式检查；私有函数加 `static`。错误码使用 `MODULE_OK/ERR_*` 规则。
