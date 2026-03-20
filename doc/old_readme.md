xmake命令
xmake project -k vsxmake -m "debug,release"
xmake project -k cmakelists

生成`compile_command.json`

```shell
xmake project -k compile_commands
```

libca库里面，base是所有的都可以的

对于嵌入式，保证`base`和`em_`前缀的库一定可以用，这些一组嵌入式组件，提供适用于嵌入式的一些常用代码和组件。

其他的库，要根据情况来看，比如需要内存管理的组件，有些情况下，嵌入式也可以非嵌入式的库，这通常会说明。



# Cauptrue Artist

This is a C++ library for application development. Simple and easy to use.

Use C++17 and static link to use it.


# Features

## base

### Result

Result is the result of a function, which can be success or failure. It is used to return the result of a function.
And in library, it is used to return the result of a function. In the process of implementing the library, I have tried to use Results as much as possible and avoided using exceptions

### Charset

This provides conversion of character encoding and mutual conversion between various miscellaneous strings in the C++ standard library.

### String

Because of C++'s `std::string` does not provide encoding, so an enhanced implementation of String with encoding is provided.

### ByteBuffer

ByteBuffer is a byte buffer that can be used to store and manipulate binary data. It is similar to `std::vector<uint8_t>`, but with additional functionality for reading and writing data.

### Format

Format is a tool that provides a way to format strings in a similar way to format string like `xxx {}`.


# How to run

test one target, xmake will build and run it.

```shell
xmake run target_name
```

if you want to run all test.

```shell
xmake test -g em/test
```

if you want to watch the output detail, add `-v` argument.

```shell
xmake test -g em/test -v
```





















libca是一个可裁剪的库，可以选择所需要的部分进行裁剪。
目前主要是针对C++17进行封装，大致思路是先拿C封装一层平台的，然后再封装一层C++的。

thread - 线程库，目前打算只提供线程和相关的组件，重新做的锁、条件变量、信号量等，提供一个简单的任务调度器

network - 对Windows和Linux下网络api的封装

collection - 集合库，让C++通过lambda来操作集合

io - 文件io库

filesystem - 文件和目录的创建、删除、遍历等操作

log - 日志库，提供简单易用的日志记录和日志输出

event - 事件库，提供一个事件系统，eventbus

utility - 实用工具拓展库，包括（ini、json等常见文件解析，一些常用的设计模式辅助类）

database - 数据库orm+连接池的实现