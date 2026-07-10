#pragma once

#include "libca/io/native_handle.hpp"
#include "libca/io/reader.hpp"
#include "libca/io/seek.hpp"
#include "libca/io/writer.hpp"

namespace ca::io {

/// @brief 基于 OwnedHandle 的同步原生字节流，实现 Reader、Writer 和 Seek。
///
/// 文件通常支持全部能力；管道可读写但 seek 返回 Unsupported。Windows SOCKET 不可交给
/// 本类型，应由 net 模块使用 closesocket()/recv()/send() 实现相同接口。
class NativeStream final : public Reader, public Writer, public Seek
{
public:
    /// @brief 接管一个 OwnedHandle。
    explicit NativeStream(OwnedHandle handle) noexcept;

    NativeStream(const NativeStream&)                = delete;
    NativeStream& operator=(const NativeStream&)     = delete;
    NativeStream(NativeStream&&) noexcept            = default;
    NativeStream& operator=(NativeStream&&) noexcept = default;
    ~NativeStream() override                         = default;

    IoResult<usize> read(u8* buffer, usize capacity) override;
    IoResult<usize> write(const u8* data, usize length) override;
    IoResult<void>  flush() override;
    IoResult<u64>   seek(const SeekFrom& position) override;

    /// @brief 是否仍拥有有效原生资源。
    bool is_open() const noexcept;

    /// @brief 返回原生值；对象仍保留所有权。
    RawHandle native_handle() const noexcept;

    /// @brief 返回拥有者引用。
    OwnedHandle&       handle() noexcept;
    const OwnedHandle& handle() const noexcept;

    /// @brief 取回原生资源所有权并使流变为空对象。
    OwnedHandle into_handle() noexcept;

private:
    OwnedHandle handle_;
};

}   // namespace ca::io
