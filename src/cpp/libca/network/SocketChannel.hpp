#ifndef LIBCA_NETWORK_SOCKET_CHANNEL_HPP
#define LIBCA_NETWORK_SOCKET_CHANNEL_HPP

#include "../io/Channel.hpp"

namespace libca {

class TcpSocketChannel : public Channel{

};

class TcpServerSocketChannel : public Channel{

};

class UdpSocketChannel : public Channel{
    
};

}   // namespace libca

#endif   // !LIBCA_NETWORK_SOCKET_CHANNEL_HPP