#pragma once

#include <mutex>
#include <memory>

namespace ca {

// ============================================================================
// Singleton — 线程安全的懒汉单例（double-checked locking）
// ============================================================================

template<class T>
class Singleton
{
public:
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

template<typename T>
class MeyersSingleton
{
public:
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
