#pragma once

#include <mutex>
#include <memory>

/// @file wrapper.hpp
/// @brief 单例模板 Singleton / MeyersSingleton。
/// @note **Legacy**：Singleton 用手写 double-checked locking + 裸指针，风险较高，且位于
///       `ca` 而非 `ca::core`，风格不统一。新代码优先用函数内 static 局部变量或依赖注入；
///       需要单例时用 MeyersSingleton。保留仅为兼容，不作为推广接口。

namespace ca {

// ============================================================================
// Singleton — 线程安全的懒汉单例（double-checked locking）
// ============================================================================

/// @brief 线程安全懒汉单例（double-checked locking）。@see wrapper.hpp 的 Legacy 说明。
template<class T>
class Singleton
{
public:
    /// 首次调用时用 args 构造实例，之后返回同一实例指针。
    template<class... Args>
    static T* getInstance(Args&&... args)
    {
        if (m_pInstance == nullptr) {
            std::lock_guard<std::mutex> lg(m_mutex);
            if (m_pInstance == nullptr)
                m_pInstance = new T(std::forward<Args>(args)...), m_autoRelease;
        }
        return m_pInstance;
    }

    Singleton()                            = delete;
    virtual ~Singleton()                   = delete;
    Singleton(const Singleton&)            = delete;
    Singleton& operator=(const Singleton&) = delete;

private:
    class AutoRelease
    {
    public:
        AutoRelease() = default;
        ~AutoRelease()
        {
            if (nullptr != m_pInstance) {
                delete m_pInstance;
                m_pInstance = nullptr;
            }
        }
    };
    static AutoRelease m_autoRelease;
    static T*          m_pInstance;
    static std::mutex  m_mutex;
};

template<class T>
T* Singleton<T>::m_pInstance = nullptr;

template<class T>
typename Singleton<T>::AutoRelease Singleton<T>::m_autoRelease;

template<class T>
std::mutex Singleton<T>::m_mutex;

// ============================================================================
// MeyersSingleton — C++11 线程安全的静态局部变量单例
// ============================================================================

/// @brief C++11 静态局部变量单例（Meyers），线程安全、无裸指针。推荐用法。
template<typename T>
class MeyersSingleton
{
public:
    /// 返回唯一实例引用（首次调用时默认构造）。
    static T& getInstance()
    {
        static T instance{};
        return instance;
    }

private:
    MeyersSingleton()                                  = default;
    ~MeyersSingleton()                                 = default;
    MeyersSingleton(const MeyersSingleton&)            = delete;
    MeyersSingleton& operator=(const MeyersSingleton&) = delete;
};

} // namespace ca
