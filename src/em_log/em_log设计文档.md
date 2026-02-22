## em_log设计文档

em_log分为多种实现，我只规定了用户使用时候的接口的抽象语义，而不规定初始化以及一些使用的细节变化。

日志等级我们分为5个等级，分别是error, warn, info, debug，raw。raw算是一个特殊等级，它不带换行符，用于一些需要打印一些特殊字符串的场景。

```c
// 日志输出，内置带换行
#define log_error(fmt, ...)
#define log_warn(fmt, ...)
#define log_info(fmt, ...) 
#define log_debug(fmt, ...)
// 日志输出，无内置换行
#define log_raw(fmt, ...)
```

### simple_logger

simple_logger是最简单的一个实现，后端可以接入stdio或者uart，全局唯一的阻塞式日志器，这样子我们就有一个足够用的日志库了。也没有必要搞一堆复杂东西，因为这个很简单，而且足够使用，甚至无需定义tag，当然tag也可以启用。

初始化时候的代码

```c
void output_func(const u8 *buf, usize len);

slog_inif(output_func);
```

使用时候的代码，例如`dht11.c`内部

```c
#define LOG_MODULE_NAME "dht11"
#include "dht11.h"
#include "simple_logger.h"

void dht11_init(void)
{
    log_info("dht11 init");
}

```

并且这个库应该是可以配置的，可以配置的功能列表。

- 日志等级信息，可以选择为精简模式（只有首字母），也可以全称。默认是全称模式。
- 静态的日志等级过滤，默认都不被过滤
- 动态日志等级过滤，默认不开启
- tag是否开启，默认开启
- debug等级下是否输出文件名和行号，默认开启

