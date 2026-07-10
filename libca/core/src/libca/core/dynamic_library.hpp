#pragma once

#include <string>
#include <type_traits>
#include <utility>

#include "libca/core/status.hpp"

namespace ca::core {

/// @brief 跨平台动态库句柄。析构时自动卸载，不能复制但可以移动。
class DynamicLibrary
{
public:
    DynamicLibrary() = default;
    ~DynamicLibrary();

    DynamicLibrary(const DynamicLibrary&)            = delete;
    DynamicLibrary& operator=(const DynamicLibrary&) = delete;

    DynamicLibrary(DynamicLibrary&& other) noexcept;
    DynamicLibrary& operator=(DynamicLibrary&& other) noexcept;

    /// @brief 从 UTF-8 路径加载动态库。
    static StatusResult<DynamicLibrary> load(const std::string& path);

    /// @brief 查找导出的函数符号。
    /// @tparam T 导出函数的函数类型，例如 `i32()`，而不是函数指针类型。
    /// @return 成功返回函数指针。调用方必须在本对象卸载或销毁前停止使用该指针。
    template<typename T>
    StatusResult<T*> lookup(const std::string& symbol_name) const
    {
        static_assert(std::is_function<T>::value,
                      "DynamicLibrary::lookup<T> requires a function type, such as i32()");

        auto result = lookup_raw(symbol_name);
        if (result.is_err()) {
            return Err(result.unwrap_err());
        }
        return Ok(reinterpret_cast<T*>(result.unwrap()));
    }

    /// @brief 释放动态库。重复调用和对空对象调用均安全。
    void unload() noexcept;

    /// @brief 是否仍持有已加载的动态库。
    bool is_loaded() const noexcept;

private:
    explicit DynamicLibrary(void* handle) noexcept;

    StatusResult<void*> lookup_raw(const std::string& symbol_name) const;

    void* handle_{nullptr};
};

}   // namespace ca::core
