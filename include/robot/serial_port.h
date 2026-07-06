#pragma once

#include <optional>
#include <string>

namespace robot {

class SerialPort {
public:
    SerialPort() = default;
    ~SerialPort();

    SerialPort(const SerialPort&) = delete;
    SerialPort& operator=(const SerialPort&) = delete;

    bool open(const std::string& device, int baud_rate);
    bool write_line(const std::string& line);
    std::optional<std::string> read_line();
    void close();

private:
    int fd_ = -1;
    std::string rx_buffer_;
};

} // namespace robot
