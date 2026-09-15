#pragma once

#include <chrono>
#include <string>
#include <vector>

#include "libca/io/error.hpp"
#include "libca/net/address.hpp"
#include "libca/net/socket.hpp"

namespace ca::net {

/// @brief TCP keepalive 的空闲/间隔/次数三参数。
///
/// 平台差异：POSIX 通过 TCP_KEEPIDLE / TCP_KEEPINTVL / TCP_KEEPCNT 完整支持三参数；
/// Windows 的 SIO_KEEPALIVE_VALS 只支持 idle 与 interval 两参数，count 被忽略
/// （Windows 的探测次数由系统固定，无法配置）。
struct TcpKeepaliveConfig
{
    /// 连接空闲多久后发出第一个 keepalive 探测，必须大于 0。
    std::chrono::seconds idle{7200};

    /// 两个 keepalive 探测之间的间隔，必须大于 0。
    std::chrono::seconds interval{75};

    /// 判定连接死亡所需的探测失败次数，必须大于 0；Windows 忽略该参数。
    u32 count{9};
};

/// @brief 探测访问 peer 使用的本机出口 IP。
///
/// 建一个 UDP socket connect() 到 peer（不发送任何数据），再用 getsockname() 读取
/// 内核为本侧选择的地址，从而拿到按路由表选出的出口 IP。
/// @param peer 探测目标，如 `SocketAddress(IpAddress::v4(8, 8, 8, 8), 80)`。
/// @note 任何一步失败（无外网、无路由、socket 创建失败）都回落 loopback：
///       peer 为 IPv6 时返回 `::1`，否则返回 `127.0.0.1`，因此不返回错误。
IpAddress get_local_ip(const SocketAddress& peer);

/// @brief 按地址族用默认探测地址探测本机出口 IP。
///
/// 默认探测地址：IPv4 为 `8.8.8.8:80`，IPv6 为 `[2001:4860:4860::8888]:80`；
/// 失败回落规则同 get_local_ip(const SocketAddress&)。
/// @param family 探测的地址族；Unspecified 按 IPv4 处理。
IpAddress get_local_ip(AddressFamily family = AddressFamily::Ipv4);

/// @brief 在 socket 句柄上启用 SO_KEEPALIVE 并设置 keepalive 三参数。
/// @param socket 已创建的 TCP socket 句柄（不必已连接）。
/// @param config 三参数配置；idle/interval/count 任一不大于 0 返回 InvalidInput，
///               超出平台取值上限同样返回 InvalidInput。
/// @note Windows 只应用 idle 与 interval；count 被忽略但仍参与参数校验，
///       以保持跨平台语义一致。
io::IoResult<void> set_tcp_keepalive(RawSocket socket, const TcpKeepaliveConfig& config);

/// @brief 读取 socket 的 SO_KEEPALIVE 开关状态。
/// @note 只报告开关状态；keepalive 参数在 Windows 上无法回读。
io::IoResult<bool> tcp_keepalive_enabled(RawSocket socket);

/// @brief 网络接口上的一个单播地址条目。
///
/// 一个接口可能有多个地址，因此枚举结果以“接口 + 地址”为粒度展开；
/// 地址族由 address.version() 表达，不再单列字段。
struct NetworkInterfaceAddress
{
    /// 接口名：POSIX 为 ifa_name；Windows 为适配器 FriendlyName 的 UTF-8 文本，
    /// 转换失败时退回 ASCII 形式的 AdapterName。
    std::string name;

    /// 接口上的单播地址；版本即地址族。
    IpAddress address;

    /// 接口是否处于 up 状态（POSIX IFF_UP；Windows IfOperStatusUp）。
    bool is_up{false};

    /// 是否为 loopback 接口（POSIX IFF_LOOPBACK；Windows IF_TYPE_SOFTWARE_LOOPBACK）。
    bool is_loopback{false};
};

/// @brief 枚举本机网络接口的单播地址条目。
///
/// POSIX 使用 getifaddrs()（RAII freeifaddrs），Windows 使用 GetAdaptersAddresses()
/// （自管缓冲区，无需 FreeMibTable）。只保留 IPv4 / IPv6 单播地址条目，
/// 无法解析的地址被跳过；平台特有字段（MAC、MTU、网关等）不在交集内，不予暴露。
io::IoResult<std::vector<NetworkInterfaceAddress>> interface_list();

}   // namespace ca::net
