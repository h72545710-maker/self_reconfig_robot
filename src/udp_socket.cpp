#include "robot/udp_socket.h"

#include <array>
#include <cerrno>
#include <cstring>
#include <iostream>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <arpa/inet.h>
#include <fcntl.h>
#include <netdb.h>
#include <sys/socket.h>
#include <unistd.h>
#endif

namespace robot {

namespace {

#ifdef _WIN32

struct WinsockInit {
    WinsockInit() {
        WSADATA data{};
        if (WSAStartup(MAKEWORD(2, 2), &data) != 0) {
            std::cerr << "WSAStartup failed\n";
        }
    }

    ~WinsockInit() {
        WSACleanup();
    }
};

void ensure_socket_runtime() {
    static WinsockInit init;
}

constexpr std::intptr_t invalid_socket_value = -1;

bool would_block() {
    const int err = WSAGetLastError();
    return err == WSAEWOULDBLOCK;
}

#else

void ensure_socket_runtime() {}

constexpr std::intptr_t invalid_socket_value = -1;

bool would_block() {
    return errno == EAGAIN || errno == EWOULDBLOCK;
}

#endif

} // namespace

UdpSocket::UdpSocket() : fd_(invalid_socket_value) {}

UdpSocket::~UdpSocket() {
    close();
}

bool UdpSocket::open() {
    ensure_socket_runtime();
    fd_ = static_cast<NativeSocket>(::socket(AF_INET, SOCK_DGRAM, 0));
    return fd_ != invalid_socket_value;
}

bool UdpSocket::bind(std::uint16_t port) {
    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port = htons(port);
#ifdef _WIN32
    return ::bind(static_cast<SOCKET>(fd_), reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) == 0;
#else
    return ::bind(fd_, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) == 0;
#endif
}

bool UdpSocket::set_nonblocking() {
#ifdef _WIN32
    u_long mode = 1;
    return ioctlsocket(static_cast<SOCKET>(fd_), FIONBIO, &mode) == 0;
#else
    const int flags = fcntl(fd_, F_GETFL, 0);
    if (flags < 0) {
        return false;
    }
    return fcntl(fd_, F_SETFL, flags | O_NONBLOCK) == 0;
#endif
}

bool UdpSocket::send_to(const std::string& host, std::uint16_t port, const std::string& data) {
    addrinfo hints{};
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_DGRAM;

    addrinfo* result = nullptr;
    const std::string port_str = std::to_string(port);
    if (getaddrinfo(host.c_str(), port_str.c_str(), &hints, &result) != 0 || result == nullptr) {
        return false;
    }

#ifdef _WIN32
    const int sent = ::sendto(static_cast<SOCKET>(fd_),
                              data.data(),
                              static_cast<int>(data.size()),
                              0,
                              result->ai_addr,
                              static_cast<int>(result->ai_addrlen));
#else
    const int sent = ::sendto(fd_,
                              data.data(),
                              data.size(),
                              0,
                              result->ai_addr,
                              result->ai_addrlen);
#endif
    freeaddrinfo(result);
    return sent == static_cast<int>(data.size());
}

bool UdpSocket::receive_from(std::string& data, UdpEndpoint& endpoint) {
    std::array<char, 1024> buffer{};
    sockaddr_storage from{};
#ifdef _WIN32
    int from_len = sizeof(from);
    const int received = ::recvfrom(static_cast<SOCKET>(fd_),
                                    buffer.data(),
                                    static_cast<int>(buffer.size() - 1),
                                    0,
                                    reinterpret_cast<sockaddr*>(&from),
                                    &from_len);
#else
    socklen_t from_len = sizeof(from);
    const int received = ::recvfrom(fd_,
                                    buffer.data(),
                                    buffer.size() - 1,
                                    0,
                                    reinterpret_cast<sockaddr*>(&from),
                                    &from_len);
#endif

    if (received < 0) {
        if (would_block()) {
            return false;
        }
        return false;
    }

    char host[NI_MAXHOST]{};
    char service[NI_MAXSERV]{};
    if (getnameinfo(reinterpret_cast<sockaddr*>(&from),
                    from_len,
                    host,
                    sizeof(host),
                    service,
                    sizeof(service),
                    NI_NUMERICHOST | NI_NUMERICSERV) == 0) {
        endpoint.host = host;
        endpoint.port = static_cast<std::uint16_t>(std::stoi(service));
    }

    data.assign(buffer.data(), static_cast<std::size_t>(received));
    return true;
}

void UdpSocket::close() {
    if (fd_ == invalid_socket_value) {
        return;
    }
#ifdef _WIN32
    closesocket(static_cast<SOCKET>(fd_));
#else
    ::close(fd_);
#endif
    fd_ = invalid_socket_value;
}

} // namespace robot
