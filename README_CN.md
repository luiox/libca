
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