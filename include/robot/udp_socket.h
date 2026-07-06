#pragma once

#include <cstdint>
#include <string>

namespace robot {

struct UdpEndpoint {
    std::string host;
    std::uint16_t port = 0;
};

class UdpSocket {
public:
    UdpSocket();
    ~UdpSocket();

    UdpSocket(const UdpSocket&) = delete;
    UdpSocket& operator=(const UdpSocket&) = delete;

    bool open();
    bool bind(std::uint16_t port);
    bool set_nonblocking();
    bool send_to(const std::string& host, std::uint16_t port, const std::string& data);
    bool receive_from(std::string& data, UdpEndpoint& endpoint);
    void close();

private:
    using NativeSocket = std::intptr_t;

    NativeSocket fd_;
};

} // namespace robot
