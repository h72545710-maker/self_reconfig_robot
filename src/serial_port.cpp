#include "robot/serial_port.h"

#include <cerrno>
#include <cstring>
#include <iostream>

#if defined(__unix__) || defined(__APPLE__)
#include <fcntl.h>
#include <termios.h>
#include <unistd.h>
#endif

namespace robot {

namespace {

#if defined(__unix__) || defined(__APPLE__)
speed_t to_speed(int baud_rate) {
    switch (baud_rate) {
    case 9600:
        return B9600;
    case 19200:
        return B19200;
    case 38400:
        return B38400;
    case 57600:
        return B57600;
    case 115200:
        return B115200;
    case 230400:
        return B230400;
    default:
        return B115200;
    }
}
#endif

} // namespace

SerialPort::~SerialPort() {
    close();
}

bool SerialPort::open(const std::string& device, int baud_rate) {
#if defined(__unix__) || defined(__APPLE__)
    close();

    fd_ = ::open(device.c_str(), O_RDWR | O_NOCTTY | O_NONBLOCK);
    if (fd_ < 0) {
        std::cerr << "failed to open serial port " << device << ": " << std::strerror(errno) << '\n';
        return false;
    }

    termios tty{};
    if (tcgetattr(fd_, &tty) != 0) {
        std::cerr << "failed to read serial attributes: " << std::strerror(errno) << '\n';
        close();
        return false;
    }

    cfmakeraw(&tty);
    const speed_t speed = to_speed(baud_rate);
    cfsetispeed(&tty, speed);
    cfsetospeed(&tty, speed);

    tty.c_cflag |= static_cast<unsigned int>(CLOCAL | CREAD);
    tty.c_cflag &= static_cast<unsigned int>(~PARENB);
    tty.c_cflag &= static_cast<unsigned int>(~CSTOPB);
    tty.c_cflag &= static_cast<unsigned int>(~CSIZE);
    tty.c_cflag |= CS8;
    tty.c_cflag &= static_cast<unsigned int>(~CRTSCTS);

    tty.c_cc[VMIN] = 0;
    tty.c_cc[VTIME] = 0;

    if (tcsetattr(fd_, TCSANOW, &tty) != 0) {
        std::cerr << "failed to configure serial port: " << std::strerror(errno) << '\n';
        close();
        return false;
    }

    tcflush(fd_, TCIOFLUSH);
    return true;
#else
    (void)device;
    (void)baud_rate;
    std::cerr << "serial port is only implemented for Linux/macOS builds\n";
    return false;
#endif
}

bool SerialPort::write_line(const std::string& line) {
#if defined(__unix__) || defined(__APPLE__)
    if (fd_ < 0) {
        return false;
    }

    std::string bytes = line;
    if (bytes.empty() || bytes.back() != '\n') {
        bytes += "\r\n";
    }

    const char* cursor = bytes.data();
    std::size_t remaining = bytes.size();
    while (remaining > 0) {
        const ssize_t written = ::write(fd_, cursor, remaining);
        if (written < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                continue;
            }
            std::cerr << "serial write failed: " << std::strerror(errno) << '\n';
            return false;
        }
        cursor += written;
        remaining -= static_cast<std::size_t>(written);
    }
    return true;
#else
    (void)line;
    return false;
#endif
}

std::optional<std::string> SerialPort::read_line() {
#if defined(__unix__) || defined(__APPLE__)
    if (fd_ < 0) {
        return std::nullopt;
    }

    char buffer[128];
    while (true) {
        const ssize_t count = ::read(fd_, buffer, sizeof(buffer));
        if (count > 0) {
            rx_buffer_.append(buffer, static_cast<std::size_t>(count));
            continue;
        }
        if (count < 0 && errno != EAGAIN && errno != EWOULDBLOCK) {
            std::cerr << "serial read failed: " << std::strerror(errno) << '\n';
        }
        break;
    }

    const std::size_t pos = rx_buffer_.find_first_of("\r\n");
    if (pos == std::string::npos) {
        return std::nullopt;
    }

    std::string line = rx_buffer_.substr(0, pos);
    const std::size_t next = rx_buffer_.find_first_not_of("\r\n", pos);
    rx_buffer_.erase(0, next == std::string::npos ? rx_buffer_.size() : next);
    return line;
#else
    return std::nullopt;
#endif
}

void SerialPort::close() {
#if defined(__unix__) || defined(__APPLE__)
    if (fd_ >= 0) {
        ::close(fd_);
        fd_ = -1;
    }
#endif
}

} // namespace robot
