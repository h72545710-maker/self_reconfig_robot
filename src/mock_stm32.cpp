#include <cerrno>
#include <algorithm>
#include <chrono>
#include <cstring>
#include <fcntl.h>
#include <iostream>
#include <sstream>
#include <string>
#include <thread>
#include <unistd.h>

#if defined(__linux__)
#include <cstdlib>
#include <pty.h>
#include <sys/stat.h>
#include <termios.h>
#endif

namespace {

struct Config {
    std::string link_path = "/tmp/self_reconfig_stm32";
    int start_x = 0;
    int start_y = 0;
    int battery = 90;
    int fault_after_sec = 0;
};

Config parse_args(int argc, char** argv) {
    Config config;
    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];
        if (arg == "--link" && i + 1 < argc) {
            config.link_path = argv[++i];
        } else if (arg == "--x" && i + 1 < argc) {
            config.start_x = std::stoi(argv[++i]);
        } else if (arg == "--y" && i + 1 < argc) {
            config.start_y = std::stoi(argv[++i]);
        } else if (arg == "--battery" && i + 1 < argc) {
            config.battery = std::stoi(argv[++i]);
        } else if (arg == "--fault-after-sec" && i + 1 < argc) {
            config.fault_after_sec = std::stoi(argv[++i]);
        }
    }
    return config;
}

#if defined(__linux__)
bool set_nonblocking(int fd) {
    const int flags = fcntl(fd, F_GETFL, 0);
    if (flags < 0) {
        return false;
    }
    return fcntl(fd, F_SETFL, flags | O_NONBLOCK) == 0;
}

bool write_line(int fd, const std::string& line) {
    std::string bytes = line;
    if (bytes.empty() || bytes.back() != '\n') {
        bytes += "\r\n";
    }
    return ::write(fd, bytes.data(), bytes.size()) == static_cast<ssize_t>(bytes.size());
}
#endif

} // namespace

int main(int argc, char** argv) {
#if !defined(__linux__)
    (void)argc;
    (void)argv;
    std::cerr << "mock_stm32 is only supported on Linux\n";
    return 1;
#else
    const Config config = parse_args(argc, argv);

    int master_fd = -1;
    int slave_fd = -1;
    char slave_name[128] = {};
    termios tty{};
    if (openpty(&master_fd, &slave_fd, slave_name, &tty, nullptr) != 0) {
        std::cerr << "openpty failed: " << std::strerror(errno) << '\n';
        return 1;
    }
    close(slave_fd);

    if (!set_nonblocking(master_fd)) {
        std::cerr << "failed to set nonblocking mode: " << std::strerror(errno) << '\n';
        return 1;
    }

    unlink(config.link_path.c_str());
    if (symlink(slave_name, config.link_path.c_str()) != 0) {
        std::cerr << "failed to create symlink " << config.link_path
                  << " -> " << slave_name << ": " << std::strerror(errno) << '\n';
        return 1;
    }

    std::cout << "mock_stm32 ready\n";
    std::cout << "  device: " << slave_name << '\n';
    std::cout << "  link:   " << config.link_path << '\n';
    std::cout << "Use stm32_bridge --serial " << config.link_path << '\n';

    int x = config.start_x;
    int y = config.start_y;
    int battery = config.battery;
    int left_pwm = 0;
    int right_pwm = 0;
    bool fault_sent = false;
    std::string rx_buffer;
    const auto start = std::chrono::steady_clock::now();
    auto last_odom = start;

    while (true) {
        char buffer[256];
        while (true) {
            const ssize_t n = read(master_fd, buffer, sizeof(buffer));
            if (n > 0) {
                rx_buffer.append(buffer, static_cast<std::size_t>(n));
                continue;
            }
            if (n < 0 && errno != EAGAIN && errno != EWOULDBLOCK) {
                std::cerr << "read failed: " << std::strerror(errno) << '\n';
            }
            break;
        }

        while (true) {
            const auto pos = rx_buffer.find_first_of("\r\n");
            if (pos == std::string::npos) {
                break;
            }
            const std::string line = rx_buffer.substr(0, pos);
            const auto next = rx_buffer.find_first_not_of("\r\n", pos);
            rx_buffer.erase(0, next == std::string::npos ? rx_buffer.size() : next);
            if (line.empty()) {
                continue;
            }

            std::cout << "[upper->stm32] " << line << '\n';
            std::istringstream in(line);
            std::string cmd;
            in >> cmd;
            if (cmd == "PING") {
                write_line(master_fd, "PONG");
            } else if (cmd == "STOP") {
                left_pwm = 0;
                right_pwm = 0;
                write_line(master_fd, "ACK STOP");
            } else if (cmd == "VEL") {
                in >> left_pwm >> right_pwm;
                write_line(master_fd, "ACK VEL");
            }
        }

        const auto now = std::chrono::steady_clock::now();
        if (std::chrono::duration_cast<std::chrono::milliseconds>(now - last_odom).count() >= 500) {
            if (left_pwm != 0 || right_pwm != 0) {
                ++x;
                if (x % 4 == 0) {
                    ++y;
                }
                battery = std::max(0, battery - 1);
            }
            std::ostringstream odom;
            odom << "ODOM " << x << ' ' << y << " 0.0 " << battery;
            write_line(master_fd, odom.str());
            std::cout << "[stm32->upper] " << odom.str() << '\n';
            last_odom = now;
        }

        if (config.fault_after_sec > 0 && !fault_sent &&
            std::chrono::duration_cast<std::chrono::seconds>(now - start).count() >= config.fault_after_sec) {
            write_line(master_fd, "FAULT MOTOR_STALL");
            std::cout << "[stm32->upper] FAULT MOTOR_STALL\n";
            fault_sent = true;
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(20));
    }
#endif
}
