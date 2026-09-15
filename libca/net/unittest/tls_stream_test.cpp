#include <gtest/gtest.h>

#include "libca/net/tls_stream.hpp"

// 测试专用内存双工流（net/test 目录在 unittest target 的 include path 上）。
#include "mem_duplex.hpp"

#if defined(LIBCA_NET_HAS_OPENSSL)

#    include <chrono>
#    include <filesystem>
#    include <future>
#    include <string>
#    include <thread>
#    include <utility>
#    include <vector>

#    include "libca/net/tls_stream.hpp"

namespace ca::net::test {
namespace {

// FNV-1a 64：大块数据校验和，防止解密空转或数据损坏未被察觉。
u64 fnv1a(const u8* data, usize size)
{
    u64 hash = 14695981039346656037ULL;
    for (usize index = 0; index < size; ++index) {
        hash ^= data[index];
        hash *= 1099511628211ULL;
    }
    return hash;
}

// 测试资产目录：xmake 的 set_rundir 为 $(projectdir)，故相对项目根定位。
std::string asset(const char* name)
{
    const std::filesystem::path dir("libca/net/test");
    return (dir / name).string();
}

// 默认 client 配置：信任测试 CA，证书校验与主机名验证保持默认开启。
TlsClientOptions trusted_client_options()
{
    TlsClientOptions options;
    options.ca_file = asset("tls_test_ca.pem");
    return options;
}

TlsServerContext make_server_context()
{
    TlsServerOptions options;
    options.certificate_chain_file = asset("tls_test_server_cert.pem");
    options.private_key_file       = asset("tls_test_server_key.pem");
    TlsServerContext               context;
    auto                           loaded = context.load(options);
    EXPECT_TRUE(loaded.is_ok()) << (loaded.is_err() ? loaded.unwrap_err().to_string() : "");
    return context;
}

// 一对通过内存流自环完成握手的 TLS 连接：server 在独立线程 accept（阻塞内存流），
// client 在当前线程 connect。client 失败时关闭写方向以解除 server 端阻塞。
io::IoResult<std::pair<TlsStream, TlsStream>> handshake_pair(MemDuplex&                link,
                                                             const std::string&       host,
                                                             const TlsClientOptions&  client_options,
                                                             const TlsServerContext& server_context)
{
    std::promise<io::IoResult<TlsStream>> server_done;
    std::thread                           server_thread([&] {
        auto accepted = TlsStream::accept(server_context, link.b(), link.b());
        server_done.set_value(std::move(accepted));
    });

    auto client = TlsStream::connect(link.a(), link.a(), host, client_options);
    if (client.is_err())
        link.a().shutdown_write();
    auto server = server_done.get_future().get();
    server_thread.join();

    if (client.is_err())
        return ca::core::Err(std::move(client).unwrap_err());
    if (server.is_err())
        return ca::core::Err(std::move(server).unwrap_err());
    return ca::core::Ok(std::make_pair(std::move(client).unwrap(), std::move(server).unwrap()));
}

// client → server 单向数据往返断言（读循环直到收满）。
void expect_transfer(TlsStream& sender, TlsStream& receiver, std::string_view payload)
{
    auto sent = sender.write(reinterpret_cast<const u8*>(payload.data()), payload.size());
    ASSERT_TRUE(sent.is_ok()) << sent.unwrap_err().to_string();
    EXPECT_EQ(sent.unwrap(), payload.size());

    std::string received;
    u8          buffer[512];
    while (received.size() < payload.size()) {
        auto count = receiver.read(buffer, sizeof(buffer));
        ASSERT_TRUE(count.is_ok()) << count.unwrap_err().to_string();
        ASSERT_GT(count.unwrap(), 0U);
        received.append(reinterpret_cast<const char*>(buffer), count.unwrap());
    }
    EXPECT_EQ(received, payload);
}

TEST(TlsStreamTest, HandshakeAndBidirectionalEcho)
{
    auto            context = make_server_context();
    MemDuplex       link;
    auto            pair = handshake_pair(link, "localhost", trusted_client_options(), context);
    ASSERT_TRUE(pair.is_ok()) << pair.unwrap_err().to_string();
    auto secured           = std::move(pair).unwrap();
    auto& [client, server] = secured;
    ASSERT_TRUE(client.is_open());
    ASSERT_TRUE(server.is_open());

    expect_transfer(client, server, "ping over tls");
    expect_transfer(server, client, "pong back");

    // 干净关闭：close_notify 之后对端读到数据末尾即 EOF(0)。
    auto closed = client.shutdown();
    ASSERT_TRUE(closed.is_ok()) << closed.unwrap_err().to_string();
    u8 buffer[64];
    auto drained = server.read(buffer, sizeof(buffer));
    ASSERT_TRUE(drained.is_ok()) << drained.unwrap_err().to_string();
    EXPECT_EQ(drained.unwrap(), 0U);
}

TEST(TlsStreamTest, TransfersLargePayloadInFragments)
{
    auto        context = make_server_context();
    MemDuplex   link;
    link.a().set_read_chunk(1024);   // 强制底层密文小片搬运，验证多次灌入与记录边界
    auto pair = handshake_pair(link, "localhost", trusted_client_options(), context);
    ASSERT_TRUE(pair.is_ok()) << pair.unwrap_err().to_string();
    auto secured           = std::move(pair).unwrap();
    auto& [client, server] = secured;

    // 4 MiB 伪随机确定性模式：FNV-1a 校验和防止空转或数据损坏。
    constexpr usize kPayloadSize = 4U * 1024U * 1024U;
    std::vector<u8> payload(kPayloadSize);
    for (usize index = 0; index < payload.size(); ++index)
        payload[index] = static_cast<u8>(index * 31U + (index >> 9));
    auto expected_checksum = fnv1a(payload.data(), payload.size());

    auto writer = std::thread([&client, &payload] {
        auto written = client.write_all(payload.data(), payload.size());
        EXPECT_TRUE(written.is_ok()) << (written.is_err() ? written.unwrap_err().to_string() : "");
        client.flush();
    });

    u64   checksum = 14695981039346656037ULL;
    usize total    = 0;
    u8    buffer[8192];
    while (total < payload.size()) {
        auto count = server.read(buffer, sizeof(buffer));
        ASSERT_TRUE(count.is_ok()) << count.unwrap_err().to_string();
        ASSERT_GT(count.unwrap(), 0U);
        for (usize index = 0; index < count.unwrap(); ++index) {
            checksum ^= buffer[index];
            checksum *= 1099511628211ULL;
        }
        total += count.unwrap();
    }
    writer.join();
    EXPECT_EQ(total, payload.size());
    EXPECT_EQ(checksum, expected_checksum);
}

TEST(TlsStreamTest, RejectsHostnameMismatchByDefault)
{
    auto      context = make_server_context();
    MemDuplex link;
    // 证书 SAN 仅为 DNS:localhost；用别的主机名连接必须在默认校验下被拒绝。
    auto pair = handshake_pair(link, "wrong.example.com", trusted_client_options(), context);
    ASSERT_TRUE(pair.is_err());
    const auto& error = pair.unwrap_err();
    EXPECT_EQ(error.kind(), io::IoErrorKind::InvalidData);
    EXPECT_NE(error.message().find("certificate"), std::string::npos) << error.message();
}

TEST(TlsStreamTest, RejectsUntrustedCertificateChain)
{
    auto      context = make_server_context();
    MemDuplex link;
    // 信任了一个毫不相关的 CA：证书链验证必然失败。
    TlsClientOptions options;
    options.ca_file = asset("tls_test_untrusted_ca.pem");
    auto pair       = handshake_pair(link, "localhost", options, context);
    ASSERT_TRUE(pair.is_err());
    const auto& error = pair.unwrap_err();
    EXPECT_EQ(error.kind(), io::IoErrorKind::InvalidData);
    EXPECT_NE(error.message().find("certificate"), std::string::npos) << error.message();
}

TEST(TlsStreamTest, AllowsExplicitlyDisabledVerification)
{
    auto      context = make_server_context();
    MemDuplex link;
    TlsClientOptions options;
    options.verify_peer = false;   // 显式 opt-out：高风险，仅测试使用
    auto pair           = handshake_pair(link, "anything.example.com", options, context);
    ASSERT_TRUE(pair.is_ok()) << pair.unwrap_err().to_string();
    auto secured = std::move(pair).unwrap();
    expect_transfer(secured.first, secured.second, "insecure but works");
}

TEST(TlsStreamTest, ReportsUncleanEofWhenPeerClosesWithoutCloseNotify)
{
    auto      context = make_server_context();
    MemDuplex link;
    auto      pair = handshake_pair(link, "localhost", trusted_client_options(), context);
    ASSERT_TRUE(pair.is_ok()) << pair.unwrap_err().to_string();
    auto secured           = std::move(pair).unwrap();
    auto& [client, server] = secured;

    // 模拟对端 TCP FIN 而未发 close_notify：读方应得到 UnexpectedEof 而非 EOF。
    link.a().shutdown_write();
    u8  buffer[64];
    auto failed = server.read(buffer, sizeof(buffer));
    ASSERT_TRUE(failed.is_err());
    EXPECT_EQ(failed.unwrap_err().kind(), io::IoErrorKind::UnexpectedEof);
}

TEST(TlsStreamTest, PeerAbortDuringHandshakeSurfacesUncleanEof)
{
    auto      context = make_server_context();
    MemDuplex link;
    // 对端在握手开始前就断开：client 的握手循环读到底层 EOF。
    link.b().shutdown_write();
    auto client = TlsStream::connect(link.a(), link.a(), "localhost", trusted_client_options());
    ASSERT_TRUE(client.is_err());
    EXPECT_EQ(client.unwrap_err().kind(), io::IoErrorKind::UnexpectedEof);
}

TEST(TlsStreamTest, CleanCloseNotifySurfacesAsEofAfterPendingData)
{
    auto      context = make_server_context();
    MemDuplex link;
    auto      pair = handshake_pair(link, "localhost", trusted_client_options(), context);
    ASSERT_TRUE(pair.is_ok()) << pair.unwrap_err().to_string();
    auto secured           = std::move(pair).unwrap();
    auto& [client, server] = secured;

    // expect_transfer 已在 server 侧读净 shutdown 前的数据；此后的读应命中
    // close_notify 并表现为干净 EOF(0)，而不是 UnexpectedEof。
    ASSERT_TRUE(client.shutdown().is_ok());

    u8 buffer[64];
    auto eof = server.read(buffer, sizeof(buffer));
    ASSERT_TRUE(eof.is_ok()) << eof.unwrap_err().to_string();
    EXPECT_EQ(eof.unwrap(), 0U);
}

TEST(TlsStreamTest, AlpnDefaultsToNoSelectionWithoutServerCallback)
{
    auto      context = make_server_context();
    MemDuplex link;
    TlsClientOptions options    = trusted_client_options();
    options.alpn_protocols      = {"http/1.1"};
    auto pair = handshake_pair(link, "localhost", options, context);
    ASSERT_TRUE(pair.is_ok()) << pair.unwrap_err().to_string();
    EXPECT_FALSE(std::move(pair).unwrap().first.alpn_selected().has_value());
}

TEST(TlsStreamTest, ServerContextLoadRejectsMissingCertificate)
{
    TlsServerOptions options;
    options.certificate_chain_file = asset("no_such_cert.pem");
    options.private_key_file       = asset("no_such_key.pem");
    TlsServerContext               context;
    auto                           loaded = context.load(options);
    ASSERT_TRUE(loaded.is_err());
    EXPECT_EQ(loaded.unwrap_err().kind(), io::IoErrorKind::InvalidInput);
}

}   // namespace
}   // namespace ca::net::test

#else

// 未启用 OpenSSL 时只验证 stub 降级路径：connect/accept/load 均返回 Unsupported。
namespace ca::net::test {

TEST(TlsStreamTest, UnsupportedWithoutOpenssl)
{
    ASSERT_FALSE(ca::net::tls_stream_supported());

    ca::net::TlsServerContext context;
    auto                      loaded = context.load(ca::net::TlsServerOptions{});
    ASSERT_TRUE(loaded.is_err());
    EXPECT_EQ(loaded.unwrap_err().kind(), io::IoErrorKind::Unsupported);

    ca::net::test::MemDuplex link;
    auto                     client = ca::net::TlsStream::connect(link.a(), link.a(), "localhost");
    ASSERT_TRUE(client.is_err());
    EXPECT_EQ(client.unwrap_err().kind(), io::IoErrorKind::Unsupported);

    auto accepted = ca::net::TlsStream::accept(context, link.a(), link.a());
    ASSERT_TRUE(accepted.is_err());
    EXPECT_EQ(accepted.unwrap_err().kind(), io::IoErrorKind::Unsupported);
}

}   // namespace ca::net::test

#endif
