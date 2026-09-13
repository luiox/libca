#pragma once

#include <memory>
#include <string>
#include <utility>

#include "libca/http/detail/client_transport.hpp"
#include "libca/http/detail/deadline_io.hpp"
#include "libca/http/http1_codec.hpp"
#include "libca/http/message.hpp"
#include "libca/http/url.hpp"
#include "libca/io/error.hpp"
#include "libca/net/tcp.hpp"

namespace ca::http::detail {

// client 与连接池共享的单条 keep-alive 连接。transport、deadline IO 与 HTTP/1 codec
// 互相持有引用，必须作为整体移动，不能拆开或拷贝；连接只能在其 codec 回到消息边界
// （响应体消费完）后才允许归还连接池。
class ClientConnection
{
public:
    ClientConnection(std::unique_ptr<ClientTransport> value, HttpScheme origin_scheme,
                     std::string origin_host, u16 origin_port, const HttpLimits& limits)
        : transport(std::move(value))
        , deadline_reader(*transport, transport->tcp_stream(),
                          origin_scheme == HttpScheme::Https)
        , deadline_writer(*transport, transport->tcp_stream(),
                          origin_scheme == HttpScheme::Https)
        , codec_reader(deadline_reader, limits)
        , codec_writer(deadline_writer)
        , scheme(origin_scheme)
        , host(std::move(origin_host))
        , port(origin_port)
    {}

    ClientConnection(const ClientConnection&)            = delete;
    ClientConnection& operator=(const ClientConnection&) = delete;
    // codec 与 deadline IO 互相持有引用且 Http1Writer 不可移动，连接整体只允许通过
    // unique_ptr 转移所有权，禁止拷贝与移动本身。

    // checkout 前的存活校验：codec 仍有预读字节视为存活（下一条请求继续消费）；否则对
    // 原始 socket 做一次非阻塞读——读到 WouldBlock/TimedOut 表示无数据待读（存活），
    // 读到 EOF、reset 或任何意外字节都表示连接已被服务器关闭或状态异常（失效）。
    // TLS 连接的探针读走一个密文字节同样正确：close_notify 密文或提前到达的数据都意味着
    // 该连接不应再被复用。
    bool probe_alive() const
    {
        if (codec_reader.buffered_len() != 0)
            return true;
        auto& stream = transport->tcp_stream();
        if (stream.set_nonblocking(true).is_err())
            return false;
        u8       first_byte = 0;
        auto     read       = stream.read(&first_byte, 1);
        const bool restored = stream.set_nonblocking(false).is_ok();
        if (!restored)
            return false;
        if (read.is_err()) {
            const auto kind = read.unwrap_err().kind();
            return kind == io::IoErrorKind::WouldBlock || kind == io::IoErrorKind::TimedOut;
        }
        return false;
    }

    std::unique_ptr<ClientTransport> transport;
    DeadlineReader                   deadline_reader;
    DeadlineWriter                   deadline_writer;
    Http1Reader                      codec_reader;
    Http1Writer                      codec_writer;
    HttpScheme                       scheme{HttpScheme::Http};
    std::string                      host;
    u16                              port{0};
};

}   // namespace ca::http::detail
