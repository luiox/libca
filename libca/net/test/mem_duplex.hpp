#pragma once

// 测试与性能基准专用的内存双工流：一对 MemDuplexEndpoint 互通（a 写入的数据从 b
// 读出，反之亦然），用于让两个 TlsStream 经 OpenSSL memory BIO 完成真实 TLS 握手
// 与数据交换（自环），不经过任何网络。不进入发布 API。

#include <algorithm>
#include <condition_variable>
#include <cstddef>
#include <memory>
#include <mutex>
#include <utility>
#include <vector>

#include "libca/core/result.hpp"
#include "libca/io/error.hpp"
#include "libca/io/reader.hpp"
#include "libca/io/writer.hpp"

namespace ca::net::test {

// 一条线程安全的无界字节管道。write 永不阻塞；read 在缓冲为空时按构造模式
// 阻塞等待（blocking_reads=true，模拟阻塞 socket）或立即返回 WouldBlock
// （blocking_reads=false，供单线程性能基准做时间切片驱动）。
class MemPipe
{
public:
    explicit MemPipe(bool blocking_reads) noexcept
        : blocking_reads_(blocking_reads)
    {}

    io::IoResult<usize> write(const u8* data, usize length)
    {
        if (length == 0)
            return ca::core::Ok<usize>(0);
        {
            std::lock_guard<std::mutex> guard(mutex_);
            buffer_.insert(buffer_.end(), data, data + length);
        }
        readable_.notify_all();
        return ca::core::Ok(length);
    }

    io::IoResult<usize> read(u8* buffer, usize capacity)
    {
        if (capacity == 0)
            return ca::core::Ok<usize>(0);
        std::unique_lock<std::mutex> lock(mutex_);
        const auto ready = [this] { return !buffer_.empty() || writer_closed_; };
        if (blocking_reads_)
            readable_.wait(lock, ready);
        else if (!ready())
            return ca::core::Err(
                io::IoError::from_kind(io::IoErrorKind::WouldBlock, "mem pipe has no data"));
        if (buffer_.empty())
            return ca::core::Ok<usize>(0);   // 写端已关闭且数据排空：EOF
        const usize available = buffer_.size();
        const usize limit     = read_chunk_ == 0 ? available : std::min(available, read_chunk_);
        const usize count     = std::min(capacity, limit);
        std::copy_n(buffer_.begin(), static_cast<std::ptrdiff_t>(count), buffer);
        buffer_.erase(buffer_.begin(), buffer_.begin() + static_cast<std::ptrdiff_t>(count));
        return ca::core::Ok(count);
    }

    /// 关闭写方向：对端把缓冲排空后 read 返回 0（EOF），等价 TCP FIN。
    void close_writer()
    {
        {
            std::lock_guard<std::mutex> guard(mutex_);
            writer_closed_ = true;
        }
        readable_.notify_all();
    }

    /// 限制单次 read 最多返回的字节数（0 表示不限制）；用于测试强制密文分片。
    void set_read_chunk(usize value)
    {
        std::lock_guard<std::mutex> guard(mutex_);
        read_chunk_ = value;
    }

private:
    std::mutex              mutex_;
    std::condition_variable readable_;
    std::vector<u8>         buffer_;
    bool                    writer_closed_{false};
    bool                    blocking_reads_{false};
    usize                   read_chunk_{0};
};

/// MemDuplex 的一个端点：读写分别接到两条方向相反的管道上。
class MemDuplexEndpoint final : public io::Reader, public io::Writer
{
public:
    MemDuplexEndpoint(std::shared_ptr<MemPipe> incoming, std::shared_ptr<MemPipe> outgoing)
        : incoming_(std::move(incoming))
        , outgoing_(std::move(outgoing))
    {}

    io::IoResult<usize> read(u8* buffer, usize capacity) override
    {
        return incoming_->read(buffer, capacity);
    }

    io::IoResult<usize> write(const u8* data, usize length) override
    {
        return outgoing_->write(data, length);
    }

    io::IoResult<void> flush() override { return ca::core::Ok(); }

    /// 关闭本端写方向；对端随后读到 EOF。用于模拟对端提前断开。
    void shutdown_write() { outgoing_->close_writer(); }

    void set_read_chunk(usize value) { incoming_->set_read_chunk(value); }

private:
    std::shared_ptr<MemPipe> incoming_;
    std::shared_ptr<MemPipe> outgoing_;
};

/// 一对互通端点。blocking_reads 同时作用于两个端点的读方向。
class MemDuplex
{
public:
    explicit MemDuplex(bool blocking_reads = true)
        : a_to_b_(std::make_shared<MemPipe>(blocking_reads))
        , b_to_a_(std::make_shared<MemPipe>(blocking_reads))
        , a_(b_to_a_, a_to_b_)
        , b_(a_to_b_, b_to_a_)
    {}

    MemDuplexEndpoint& a() noexcept { return a_; }
    MemDuplexEndpoint& b() noexcept { return b_; }

private:
    std::shared_ptr<MemPipe> a_to_b_;
    std::shared_ptr<MemPipe> b_to_a_;
    MemDuplexEndpoint        a_;
    MemDuplexEndpoint        b_;
};

}   // namespace ca::net::test
