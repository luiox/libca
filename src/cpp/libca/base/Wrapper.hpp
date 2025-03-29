#ifndef LIBCA_BASE_WRAPPER_HPP
#define LIBCA_BASE_WRAPPER_HPP

#include <mutex>
#include <memory>

namespace ca {

template<typename T>
class Ref
{
public:
    // 构造函数接受一个引用，并将其存储为指针
    Ref(T& ref)
        : ptr(&ref)
    {}

    // 重载解引用运算符，使得 Ref 可以像普通引用一样使用
    T& operator*() const { return *ptr; }

    // 重载成员访问运算符，使得 Ref 可以像普通指针一样使用
    T* operator->() const { return ptr; }

    operator T() const { return *ptr; }

    T& operator=(T value)
    {
        *ptr = value;
        return *ptr;
    }

private:
    T* ptr;   // 指向要引用的数据的指针
};

////////////////////////////////////////////////////////////////////////////////

// 智能指针

template<typename T>
class Box
{
public:
    using TPtr = T*;

private:
    TPtr ptr_;

public:
    Box() { ptr_ = nullptr; }
    explicit Box(T value) { ptr_ = new T(value); }

    ~Box() { delete ptr_; }

    Box(const Box&)            = delete;
    Box& operator=(const Box&) = delete;

    Box(Box&& other)
        : ptr_(other.ptr)
    {
        other.ptr = nullptr;
    }
    Box& operator=(Box&& other) noexcept
    {
        if (this != &other) {
            ptr_      = other.ptr;
            other.ptr = nullptr;
        }
        return *this;
    }

    T& get() const { return ptr_; }

    T& operator*() const { return *ptr_; }

    T* operator->() const { return &ptr_; }
};

////////////////////////////////////////////////////////////////////////////////

// Singleton

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

private:
    Singleton()                            = delete;
    virtual ~Singleton()                   = delete;
    Singleton(const Singleton&)            = delete;
    Singleton& operator=(const Singleton&) = delete;

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

////////////////////////////////////////////////////////////////////////////////

// NoCopyable

class NoCopyable
{
protected:
    NoCopyable() {}
    ~NoCopyable() {}

private:
    NoCopyable(const NoCopyable&);
    NoCopyable& operator=(const NoCopyable&);
};

}   // namespace ca

#endif   // !LIBCA_BASE_WRAPPER_HPP
