#pragma once

#include <chrono>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "libca/io/error.hpp"
#include "libca/io/reader.hpp"
#include "libca/io/writer.hpp"

namespace ca::net {

/// @brief 判断当前构建是否包含 OpenSSL TLS 实现（with_openssl=y）。
bool tls_stream_supported() noexcept;

/// @brief client 侧 TLS 连接配置，默认全部安全项开启。
struct TlsClientOptions
{
    /// @brief 是否校验服务端证书链与主机名，默认开启。
    /// @warning 显式置 false 会同时关闭证书链与主机名校验，等于放弃中间人防护，
    /// 仅限测试环境或调用方明确知情并自担风险的场景。
    bool verify_peer{true};

    /// @brief 可选 PEM CA bundle 文件；与 ca_directory 同时为空时使用 OpenSSL 默认信任库。
    std::string ca_file;

    /// @brief 可选 OpenSSL hashed CA 目录；与 ca_file 同时为空时使用 OpenSSL 默认信任库。
    std::string ca_directory;

    /// @brief 可选 ALPN 协议列表（如 "http/1.1"、"h2"）；为空表示不提供 ALPN 扩展。
    std::vector<std::string> alpn_protocols;
};

/// @brief server 侧 TLS 上下文配置。
struct TlsServerOptions
{
    /// @brief PEM 证书链文件路径（leaf 证书在前，后跟 intermediate）。
    std::string certificate_chain_file;

    /// @brief PEM 私钥文件路径；私钥可与证书链同文件，也可独立。
    std::string private_key_file;

    /// @brief 可选 PEM CA bundle 文件，verify_client=true 时用于校验客户端证书；
    /// 与 ca_directory 同时为空时回退到 OpenSSL 默认信任库。
    std::string ca_file;

    /// @brief 可选 OpenSSL hashed CA 目录，用于校验客户端证书。
    std::string ca_directory;

    /// @brief 是否要求并校验客户端证书（mTLS）。
    /// @note 为 true 时，无客户端证书的连接在 TLS 握手阶段即被拒绝。
    bool verify_client{false};
};

/// @brief TLS 握手过程控制参数。
struct TlsHandshakeControl
{
    /// @brief 握手总期限；在每个重试迭代边界强制检查。底层流自身的阻塞行为
    /// （无超时能力的流，如内存流）可能使单次阻塞超过该期限。
    std::chrono::milliseconds timeout{10000};

    /// @brief 每次握手重试前调用的钩子（可选；参数为剩余期限）。
    /// @details 典型用途：按剩余期限收紧底层 socket 读写超时、检查协作停止标记。
    /// 返回 Err 时立即以该错误终止握手（如 server 停止时的 ConnectionAborted）。
    std::function<io::IoResult<void>(std::chrono::milliseconds remaining)> before_retry;
};

/// @brief server 端可长期持有的 TLS 上下文，包装 OpenSSL SSL_CTX。
/// @details load() 一次，随后所有 accept() 共享；握手期间对它只读，可多线程并发。
/// 未启用 with_openssl 时 load() 返回 Unsupported。
class TlsServerContext
{
public:
    TlsServerContext() noexcept;
    ~TlsServerContext();
    TlsServerContext(const TlsServerContext&)            = delete;
    TlsServerContext& operator=(const TlsServerContext&) = delete;
    TlsServerContext(TlsServerContext&&) noexcept;
    TlsServerContext& operator=(TlsServerContext&&) noexcept;

    /// @brief 加载 PEM 证书链与私钥，并按 options 配置（mTLS）客户端证书校验。
    /// @return 配置或加载失败返回 InvalidInput；未启用 with_openssl 返回 Unsupported。
    io::IoResult<void> load(const TlsServerOptions& options);

    /// @brief 内部 SSL_CTX 句柄（未启用 with_openssl 或未 load 时返回 nullptr）。
    void* native_handle() const noexcept;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

/// @brief 基于 OpenSSL memory BIO 的 TLS 字节流，明文侧实现 io::Reader/io::Writer。
/// @details 密文经一对 memory BIO 与底层双向流（io::Reader + io::Writer）自动互搬：
/// 读时把底层流密文灌入读 BIO 再解密，写时把写 BIO 密文刷入底层流。TLS 握手与
/// renegotiation 对调用方透明，WANT_READ/WANT_WRITE 重试循环封闭在内部。
/// @note TlsStream 不拥有底层流；底层流必须比 TlsStream 存活得更久。
/// @note 错误分类：证书校验失败与 TLS 协议错误为 InvalidData（消息可区分）、
/// 对端未发 close_notify 即断开为 UnexpectedEof、干净关闭使 read 返回 0、
/// 底层流错误原样透传（保留 IoErrorKind 与原生错误码）。
class TlsStream final : public io::Reader, public io::Writer
{
public:
    /// @brief 以 client 身份在已建立的底层流上完成 TLS 握手。
    /// @param input 底层流读端（接收对端密文）。
    /// @param output 底层流写端（发出本端密文），可与 input 为同一对象。
    /// @param host 目标主机名：SNI 默认取该值（IP 字面量不发 SNI）；
    /// verify_peer=true 时同时用作证书主机名校验（自动识别 IP SAN）。
    /// @return 未启用 with_openssl 返回 Unsupported；握手失败返回对应 IoError。
    static io::IoResult<TlsStream> connect(io::Reader& input, io::Writer& output,
                                           const std::string&      host,
                                           const TlsClientOptions& options = {},
                                           const TlsHandshakeControl& control = {});

    /// @brief 以 server 身份在已接入的底层流上完成 TLS 握手。
    /// @param context 已 load() 的服务端上下文（多连接共享，只读）。
    /// @return 未 load 或未启用 with_openssl 返回 Unsupported/InvalidInput。
    static io::IoResult<TlsStream> accept(const TlsServerContext&    context,
                                          io::Reader& input, io::Writer& output,
                                          const TlsHandshakeControl& control = {});

    TlsStream(const TlsStream&)            = delete;
    TlsStream& operator=(const TlsStream&) = delete;
    TlsStream(TlsStream&& other) noexcept;
    TlsStream& operator=(TlsStream&& other) noexcept;
    ~TlsStream() override;

    /// @brief 读取解密后的明文；返回 0 表示收到对端 close_notify（干净关闭）。
    /// @note 内部自动完成密文搬运与重试；底层流的 TimedOut/WouldBlock 原样透传，
    /// 交由调用方决定是否重试。
    io::IoResult<usize> read(u8* buffer, usize capacity) override;

    /// @brief 加密并写入明文，返回实际接受的长度；密文同步刷入底层流。
    io::IoResult<usize> write(const u8* data, usize length) override;

    /// @brief 刷出未发的密文并 flush 底层流。
    io::IoResult<void> flush() override;

    /// @brief 发送 close_notify 并刷出密文（单向关闭，不等待对端回应）。
    /// @note 之后仍可读出对端在关闭前发出的数据；重复调用安全。
    io::IoResult<void> shutdown();

    /// @brief 是否已完成 TLS 握手（moved-from 状态返回 false）。
    bool is_open() const noexcept;

    /// @brief 协商出的 ALPN 协议名；对端未选择时返回 nullopt。
    std::optional<std::string> alpn_selected() const;

private:
    struct Impl;

    /// @brief 未连接状态（仅供 connect/accept 内部构造，随后立即完成握手）。
    TlsStream() noexcept;

    io::Reader*           input_{nullptr};
    io::Writer*           output_{nullptr};
    std::unique_ptr<Impl> impl_;
};

}   // namespace ca::net
