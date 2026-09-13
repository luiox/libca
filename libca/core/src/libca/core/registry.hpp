#pragma once

#include "datatype.hpp"
#include "status.hpp"

#include <functional>
#include <map>
#include <memory>
#include <mutex>
#include <utility>
#include <vector>

/// @file registry.hpp
/// @brief 泛型自注册工厂 Registry<Key, Base, Args...>。
/// @note 内部表为函数内 static（首次使用时构造），规避跨编译单元静态初始化顺序
///       问题：文件级静态注册器对象在 main 之前执行注册是安全的。

namespace ca::core {

/// @brief 泛型自注册工厂：以 Key 查找工厂并构造 Base 的派生对象。
/// @tparam Key 注册键类型，需支持 operator<（内部用 std::map 有序存储）。
/// @tparam Base 产品基类，工厂返回 std::unique_ptr<Base>。
/// @tparam Args 工厂调用参数形态：create(key, args...) 原样转发给工厂，
///              无额外构造参数时留空即可。
/// @note 全部接口为 static；每个 Registry<Key, Base, Args...> 实例化各有一张
///       独立的表，不同 <Key, Base, Args...> 组合互不干扰。接口线程安全。
/// @note 静态自注册惯用法（文件级静态注册器对象，main 之前完成注册）：
/// @code
/// // shape.hpp —— 基类 + 注册表别名
/// class Shape {
/// public:
///     virtual ~Shape() = default;
///     virtual double area() const = 0;
/// };
/// using ShapeRegistry = ca::core::Registry<std::string, Shape>;
///
/// // circle.cpp —— 匿名命名空间内的文件级静态对象，程序启动时自动注册
/// class Circle : public Shape { /* ... */ };
/// namespace {
/// const bool g_registered_circle = ShapeRegistry::register_factory(
///     "circle",
///     []() -> std::unique_ptr<Shape> { return std::make_unique<Circle>(); }).is_ok();
/// }
///
/// // main.cpp —— 按键构造，无需 include 具体实现
/// auto shape = ShapeRegistry::create("circle");   // StatusResult<std::unique_ptr<Shape>>
/// @endcode
template<typename Key, typename Base, typename... Args>
class Registry {
public:
    /// @brief 工厂类型：按注册时给定的 Args 形态构造一个 Base 对象。
    using Factory = std::function<std::unique_ptr<Base>(Args...)>;

    Registry() = delete;

    /// @brief 注册工厂。
    /// @param key 注册键；已存在时注册失败，原工厂保持不变（不覆盖）。
    /// @param factory 工厂函数；为空视为 INVALID_ARGUMENT。
    /// @return 成功返回 OK；key 重复返回 ALREADY_EXISTS。
    static Status register_factory(Key key, Factory factory) {
        std::lock_guard<std::mutex> lock(mutex());
        if (factory == nullptr) {
            return ErrStatus(StatusCode::INVALID_ARGUMENT, "registry factory must not be empty");
        }
        const auto inserted = factories().emplace(std::move(key), std::move(factory));
        if (!inserted.second) {
            return ErrStatus(StatusCode::ALREADY_EXISTS, "registry key already registered");
        }
        return OkStatus();
    }

    /// @brief 注销工厂。用于插件卸载与测试隔离。
    /// @return key 存在并成功移除返回 true；不存在返回 false。
    static bool unregister_factory(const Key& key) {
        std::lock_guard<std::mutex> lock(mutex());
        return factories().erase(key) == 1;
    }

    /// @brief 按键构造对象。
    /// @param args 转发给工厂的构造参数。
    /// @return key 不存在返回 NOT_FOUND；存在则调用工厂并返回其结果。
    /// @note 工厂在锁外调用：工厂内部可安全重入注册接口，不会死锁。
    static StatusResult<std::unique_ptr<Base>> create(const Key& key, Args... args) {
        Factory factory;
        {
            std::lock_guard<std::mutex> lock(mutex());
            const auto found = factories().find(key);
            if (found == factories().end()) {
                return ErrStatus(StatusCode::NOT_FOUND, "registry key not found");
            }
            factory = found->second;
        }
        return Ok(factory(std::forward<Args>(args)...));
    }

    /// @brief 返回全部已注册键（按 Key 升序）。
    static std::vector<Key> keys() {
        std::lock_guard<std::mutex> lock(mutex());
        std::vector<Key> result;
        result.reserve(factories().size());
        for (const auto& entry : factories()) {
            result.push_back(entry.first);
        }
        return result;
    }

    /// @brief 是否已注册指定键。
    static bool contains(const Key& key) {
        std::lock_guard<std::mutex> lock(mutex());
        return factories().find(key) != factories().end();
    }

    /// @brief 已注册的工厂数量。
    static usize size() {
        std::lock_guard<std::mutex> lock(mutex());
        return factories().size();
    }

private:
    /// @brief 注册表本体：函数内 static，首次使用时构造，规避静态初始化顺序问题。
    static std::map<Key, Factory>& factories() {
        static std::map<Key, Factory> map;
        return map;
    }

    /// @brief 保护注册表的互斥量（同为函数内 static）。
    static std::mutex& mutex() {
        static std::mutex mutex;
        return mutex;
    }
};

} // namespace ca::core
