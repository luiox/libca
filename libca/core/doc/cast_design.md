# cast 设计文档

## 定位

libtyped 是 morpher-native 中无依赖的头文件工具库，提供 LLVM 风格的轻量级 RTTI 强制转换工具。
命名空间：`typed`。

## 设计动机

morpher-native 需要处理 JVM 字节码中的多态节点层次结构（如 `AbstractInsnNode` → `InsnNode`、`VarInsnNode` 等），
C++ 原生 RTTI（`dynamic_cast` / `typeid`）过于重量级且不可靠，因此采用 LLVM 风格的枚举分派 + `static_cast` 方案。

## 核心模板

```cpp
template<typename T> struct TypeOf;  // 特化接口：每个类型关联一个枚举常量
template<typename T, typename U> bool isa(const U* val);
template<typename T, typename U> T cast(U* val);
template<typename T, typename U> T dyn_cast(U* val);
```

## API 说明

### `TypeOf<T>`

类型特征特化接口，各库为自身类型提供特化。`value` 应为枚举常量（任意枚举类型），与 `getType()` 的返回值类型一致。

`TypeOf<const T>` 自动委托给 `TypeOf<T>`。

### `isa<T>(ptr)`

类型检查。等价于 `dynamic_cast` 的条件判断部分。
通过 `ptr->getType() == TypeOf<T>::value` 比较。

### `cast<T>(ptr)`

不检查的向下转型。等价于 `static_cast`。
调用方应确保已通过 `isa<T>()` 验证过类型。
支持 `cast<Foo>(ptr)` 和 `cast<Foo*>(ptr)` 两种写法。

### `dyn_cast<T>(ptr)`

带检查的转型。等价于 `dynamic_cast`。
内部调用 `isa<T>()` + `cast<T>()`，类型不匹配时返回 `nullptr`。

## 使用方式

```cpp
// 先为具体类型特化 TypeOf
template<> struct typed::TypeOf<InsnNode> {
    static constexpr int value = AbstractInsnNode::INSN;
};

// 然后使用 RTTI 工具
if (typed::isa<InsnNode>(node)) { ... }
auto* insn = typed::cast<InsnNode>(node);
auto* maybe = typed::dyn_cast<InsnNode>(node);
```

## 文件结构

```
src/
└── typed/
    └── cast.hpp          # 全部实现（58 行，header-only）
```

## 限制

- 不支持多继承层次（线性枚举比较，无完整类型信息）
- `getType()` 必须由基类提供虚函数接口，本库不强制要求
- 无测试覆盖
