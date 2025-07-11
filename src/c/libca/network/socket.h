#ifndef LIBCA_NETWORK_SOCKET_H
#define LIBCA_NETWORK_SOCKET_H

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#pragma comment(lib, "Ws2_32.lib")
#else
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#endif

#include "libca/core/platform.h"


i32_t ca_socket_init();




#endif // LIBCA_NETWORK_SOCKET_H
