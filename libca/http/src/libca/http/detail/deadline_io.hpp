#pragma once

#include <algorithm>
#include <chrono>
#include <string>
#include <utility>

#include "libca/io/reader.hpp"
#include "libca/io/writer.hpp"
#include "libca/net/tcp.hpp"
#include "libca/thread/stop_token.hpp"

namespace ca::http::detail {

class DeadlineReader final : public io::Reader
{
public:
    explicit DeadlineReader(net::TcpStream& stream) noexcept
        : reader_(&stream)
        , stream_(&stream)
    {}

    DeadlineReader(io::Reader& reader, net::TcpStream& stream,
                   bool bidirectional_io = false) noexcept
        : reader_(&reader)
        , stream_(&stream)
        , bidirectional_io_(bidirectional_io)
    {}

    DeadlineReader(net::TcpStream& stream, ca::thread::StopToken stop_token,
                   std::chrono::milliseconds stop_poll_interval) noexcept
        : reader_(&stream)
        , stream_(&stream)
        , stop_token_(std::move(stop_token))
        , stop_poll_interval_(stop_poll_interval)
    {}

    /// @brief 抽象 transport 路径:reader 走任意 io::Reader(如 TLS transport),
    /// 超时仍通过底层 TcpStream 设置;附带 stop_token 让 server 能协作停止。
    /// @param bidirectional_io TLS 场景传 true:SSL 读写共享同一 state,
    /// 读时应同时设 write_timeout、写时应同时设 read_timeout。
    DeadlineReader(io::Reader& reader, net::TcpStream& stream, ca::thread::StopToken stop_token,
                   std::chrono::milliseconds stop_poll_interval,
                   bool                      bidirectional_io = false) noexcept
        : reader_(&reader)
        , stream_(&stream)
        , stop_token_(std::move(stop_token))
        , stop_poll_interval_(stop_poll_interval)
        , bidirectional_io_(bidirectional_io)
    {}

    void start(std::chrono::milliseconds timeout) noexcept
    {
        waiting_first_byte_ = false;
        deadline_           = std::chrono::steady_clock::now() + timeout;
    }

    void start_head(std::chrono::milliseconds idle_timeout,
                    std::chrono::milliseconds header_timeout, bool input_already_buffered) noexcept
    {
        waiting_first_byte_ = !input_already_buffered;
        continuation_       = header_timeout;
        deadline_           = std::chrono::steady_clock::now() +
                    (input_already_buffered ? header_timeout : idle_timeout);
    }

    io::IoResult<usize> read(u8* buffer, usize capacity) override
    {
        for (;;) {
            if (stop_token_.stop_requested())
                return cancelled_error("HTTP read cancelled by server stop");
            auto timeout = remaining_timeout();
            if (timeout.is_err())
                return ca::core::Err(timeout.unwrap_err());
            const auto wait = polling_enabled() ? std::min(timeout.unwrap(), stop_poll_interval_)
                                                : timeout.unwrap();
            auto       configured = stream_->set_read_timeout(wait);
            if (configured.is_err())
                return ca::core::Err(configured.unwrap_err());
            if (bidirectional_io_) {
                auto write_timeout = stream_->set_write_timeout(wait);
                if (write_timeout.is_err())
                    return ca::core::Err(write_timeout.unwrap_err());
            }
            auto result = reader_->read(buffer, capacity);
            if (result.is_err() && polling_enabled() && is_timeout(result.unwrap_err()))
                continue;
            if (result.is_ok() && result.unwrap() != 0 && waiting_first_byte_) {
                waiting_first_byte_ = false;
                deadline_           = std::chrono::steady_clock::now() + continuation_;
            }
            return result;
        }
    }

private:
    bool polling_enabled() const noexcept
    {
        return stop_token_.stop_possible() && stop_poll_interval_.count() > 0;
    }

    static bool is_timeout(const io::IoError& error) noexcept
    {
        return error.kind() == io::IoErrorKind::TimedOut ||
               error.kind() == io::IoErrorKind::WouldBlock;
    }

    static io::IoResult<usize> cancelled_error(std::string message)
    {
        return ca::core::Err(
            io::IoError::from_kind(io::IoErrorKind::ConnectionAborted, std::move(message)));
    }

    io::IoResult<std::chrono::milliseconds> remaining_timeout() const
    {
        const auto now = std::chrono::steady_clock::now();
        if (now >= deadline_)
            return ca::core::Err(
                io::IoError::from_kind(io::IoErrorKind::TimedOut, "HTTP read deadline exceeded"));
        auto remaining = std::chrono::duration_cast<std::chrono::milliseconds>(deadline_ - now);
        if (remaining.count() == 0)
            remaining = std::chrono::milliseconds(1);
        return ca::core::Ok(remaining);
    }

    io::Reader*                           reader_{nullptr};
    net::TcpStream*                       stream_{nullptr};
    std::chrono::steady_clock::time_point deadline_{};
    std::chrono::milliseconds             continuation_{0};
    bool                                  waiting_first_byte_{false};
    ca::thread::StopToken                 stop_token_;
    std::chrono::milliseconds             stop_poll_interval_{0};
    bool                                  bidirectional_io_{false};
};

class DeadlineWriter final : public io::Writer
{
public:
    explicit DeadlineWriter(net::TcpStream& stream) noexcept
        : writer_(&stream)
        , stream_(&stream)
    {}

    DeadlineWriter(io::Writer& writer, net::TcpStream& stream,
                   bool bidirectional_io = false) noexcept
        : writer_(&writer)
        , stream_(&stream)
        , bidirectional_io_(bidirectional_io)
    {}

    DeadlineWriter(net::TcpStream& stream, ca::thread::StopToken stop_token,
                   std::chrono::milliseconds stop_poll_interval) noexcept
        : writer_(&stream)
        , stream_(&stream)
        , stop_token_(std::move(stop_token))
        , stop_poll_interval_(stop_poll_interval)
    {}

    /// @brief 抽象 transport 路径,对称 DeadlineReader 的同名构造。
    DeadlineWriter(io::Writer& writer, net::TcpStream& stream, ca::thread::StopToken stop_token,
                   std::chrono::milliseconds stop_poll_interval,
                   bool                      bidirectional_io = false) noexcept
        : writer_(&writer)
        , stream_(&stream)
        , stop_token_(std::move(stop_token))
        , stop_poll_interval_(stop_poll_interval)
        , bidirectional_io_(bidirectional_io)
    {}

    void start(std::chrono::milliseconds timeout) noexcept
    {
        idle_mode_ = false;
        deadline_  = std::chrono::steady_clock::now() + timeout;
    }

    void start_idle(std::chrono::milliseconds timeout) noexcept
    {
        idle_mode_    = true;
        idle_timeout_ = timeout;
    }

    io::IoResult<usize> write(const u8* data, usize length) override
    {
        if (idle_mode_)
            deadline_ = std::chrono::steady_clock::now() + idle_timeout_;
        for (;;) {
            if (stop_token_.stop_requested())
                return cancelled_error("HTTP write cancelled by server stop");
            auto timeout = remaining_timeout();
            if (timeout.is_err())
                return ca::core::Err(timeout.unwrap_err());
            const auto wait = polling_enabled() ? std::min(timeout.unwrap(), stop_poll_interval_)
                                                : timeout.unwrap();
            auto       configured = stream_->set_write_timeout(wait);
            if (configured.is_err())
                return ca::core::Err(configured.unwrap_err());
            if (bidirectional_io_) {
                auto read_timeout = stream_->set_read_timeout(wait);
                if (read_timeout.is_err())
                    return ca::core::Err(read_timeout.unwrap_err());
            }
            auto result = writer_->write(data, length);
            if (result.is_err() && polling_enabled() && is_timeout(result.unwrap_err()))
                continue;
            return result;
        }
    }

    io::IoResult<void> flush() override { return writer_->flush(); }

private:
    bool polling_enabled() const noexcept
    {
        return stop_token_.stop_possible() && stop_poll_interval_.count() > 0;
    }

    static bool is_timeout(const io::IoError& error) noexcept
    {
        return error.kind() == io::IoErrorKind::TimedOut ||
               error.kind() == io::IoErrorKind::WouldBlock;
    }

    static io::IoResult<usize> cancelled_error(std::string message)
    {
        return ca::core::Err(
            io::IoError::from_kind(io::IoErrorKind::ConnectionAborted, std::move(message)));
    }

    io::IoResult<std::chrono::milliseconds> remaining_timeout() const
    {
        const auto now = std::chrono::steady_clock::now();
        if (now >= deadline_)
            return ca::core::Err(
                io::IoError::from_kind(io::IoErrorKind::TimedOut, "HTTP write deadline exceeded"));
        auto remaining = std::chrono::duration_cast<std::chrono::milliseconds>(deadline_ - now);
        if (remaining.count() == 0)
            remaining = std::chrono::milliseconds(1);
        return ca::core::Ok(remaining);
    }

    io::Writer*                           writer_{nullptr};
    net::TcpStream*                       stream_{nullptr};
    std::chrono::steady_clock::time_point deadline_{};
    std::chrono::milliseconds             idle_timeout_{0};
    bool                                  idle_mode_{false};
    ca::thread::StopToken                 stop_token_;
    std::chrono::milliseconds             stop_poll_interval_{0};
    bool                                  bidirectional_io_{false};
};

}   // namespace ca::http::detail
