【libca的非em_系列组件代码库规范（必须遵守）】

【语言】
- C99（在`.c`和`.h`这两种c文件内），C++17（在`.cpp`和`.hpp`这两种c++文件内）
- 在c文件内，禁止使用c++语法，并且不要用C++编译器编译C代码，只能是用c代码的接口，对于可能要被c++使用的C接口，要以下面这个包裹
```c
#ifdef __cplusplus
extern "C" {
#endif

#ifdef __cplusplus
}
#endif
```