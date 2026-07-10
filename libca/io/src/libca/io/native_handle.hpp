#pragma once

#include <cstdint>

#include "libca/io/error.hpp"

namespace ca::io {

/// @brief 跨平台原生资源值；Windows 保存 HANDLE，POSIX 保存 fd。
using RawHandle = std::intptr_t;

/// @brief Windows HANDLE 或 POSIX fd 的 move-only RAII 唯一所有者。
///
/// Windows 资源必须可由 CloseHandle() 关闭；SOCKET 不适用本类型。POSIX fd 由 close()
/// 关闭，数值 0 是有效 fd。adopt() 明确接管调用方已有资源的关闭责任。
class OwnedHandle
{
public:
    /// @brief 构造空所有者。
    OwnedHandle() noexcept = default;

    OwnedHandle(const OwnedHandle&)            = delete;
    OwnedHandle& operator=(const OwnedHandle&) = delete;
    OwnedHandle(OwnedHandle&& other) noexcept;
    OwnedHandle& operator=(OwnedHandle&& other) noexcept;
    ~OwnedHandle();

    /// @brief 接管有效原生资源；无效值返回 InvalidInput。
    static IoResult<OwnedHandle> adopt(RawHandle handle);

    /// @brief 创建指向同一 OS 对象的独立所有者。
    IoResult<OwnedHandle> duplicate() const;

    /// @brief 是否拥有有效资源。
    bool is_valid() const noexcept;

    /// @brief 返回原生值；空对象返回平台无效值。
    RawHandle get() const noexcept;

    /// @brief 显式关闭。无资源时幂等成功；关闭失败后仍放弃所有权以防重复关闭。
    IoResult<void> close();

    /// @brief 放弃 RAII 并返回原生值，调用方重新承担关闭责任。
    RawHandle release() noexcept;

private:
    explicit OwnedHandle(RawHandle handle) noexcept;
    void close_noexcept() noexcept;

    RawHandle handle_{-1};
};

}   // namespace ca::io
