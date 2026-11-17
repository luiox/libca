#pragma once

/// @file wrapper.hpp
/// @brief Meyers 单例模板。

namespace ca {

/// @brief C++11 静态局部变量单例（Meyers），线程安全、无裸指针。推荐用法。
template<typename T>
class MeyersSingleton
{
public:
    /// 返回唯一实例引用（首次调用时默认构造）。
    static T& get_instance()
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
