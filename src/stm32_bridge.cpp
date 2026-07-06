#include "robot/protocol.h"
#include "robot/serial_port.h"
#include "robot/types.h"
#include "robot/udp_socket.h"

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <iostream>
#include <optional>
#include <sstream>
#include <string>
#include <thread>

namespace {

using Clock = std::chrono::steady_clock;

struct Config {
    std::uint32_t id = 1;
    std::string master_host = "127.0.0.1";
    std::uint16_t master_port = 9000;
    std::string serial_device = "/dev/ttyUSB0";
    int baud_rate = 115200;
    int x = 0;
    int y = 0;
    int battery = 90;
    int drive_pwm = 260;
};

struct CurrentTask {
    std::uint32_t id = 0;
    robot::TaskType type = robot::TaskType::Explore;
    robot::TaskState state = robot::TaskState::Idle;
    std::uint16_t progress = 0;
    int target_x = 0;
    int target_y = 0;
};

struct IncomingCommand {
    robot::MessageType type = robot::MessageType::Heartbeat;
    std::uint32_t sequence = 0;
    robot::Role role = robot::Role::Idle;
    robot::TaskAssignPayload task{};
};

struct LowerFeedback {
    std::optional<int> x;
    std::optional<int> y;
    std::optional<int> battery;
    bool motor_fault = false;
    std::string fault_code;
};

Config parse_args(int argc, char** argv) {
    Config config;
    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];
        if (arg == "--id" && i + 1 < argc) {
            config.id = static_cast<std::uint32_t>(std::stoul(argv[++i]));
        } else if (arg == "--master" && i + 1 < argc) {
            config.master_host = argv[++i];
        } else if (arg == "--port" && i + 1 < argc) {
            config.master_port = static_cast<std::uint16_t>(std::stoi(argv[++i]));
        } else if (arg == "--serial" && i + 1 < argc) {
            config.serial_device = argv[++i];
        } else if (arg == "--baud" && i + 1 < argc) {
            config.baud_rate = std::stoi(argv[++i]);
        } else if (arg == "--x" && i + 1 < argc) {
            config.x = std::stoi(argv[++i]);
        } else if (arg == "--y" && i + 1 < argc) {
            config.y = std::stoi(argv[++i]);
        } else if (arg == "--battery" && i + 1 < argc) {
            config.battery = std::stoi(argv[++i]);
        } else if (arg == "--drive-pwm" && i + 1 < argc) {
            config.drive_pwm = std::stoi(argv[++i]);
        }
    }
    return config;
}

std::string make_status_message(robot::MessageType type, const Config& config, std::uint32_t sequence) {
    const robot::StatusPayload payload{config.x, config.y, config.battery};
    return robot::encode_message(type, config.id, sequence, robot::encode_status_payload(payload));
}

std::string make_ack_message(const Config& config, std::uint32_t sequence, std::uint32_t ack_sequence) {
    return robot::encode_message(robot::MessageType::Ack,
                                 config.id,
                                 sequence,
                                 robot::encode_ack_payload(ack_sequence));
}

std::string make_task_status_message(const Config& config,
                                     std::uint32_t sequence,
                                     const CurrentTask& task) {
    const robot::TaskStatusPayload payload{task.id, task.state, task.progress};
    return robot::encode_message(robot::MessageType::TaskStatus,
                                 config.id,
                                 sequence,
                                 robot::encode_task_status_payload(payload));
}

std::string make_fault_message(const Config& config, std::uint32_t sequence) {
    return robot::encode_message(robot::MessageType::FaultReport, config.id, sequence, {});
}

std::optional<IncomingCommand> parse_command(const std::string& data, std::uint32_t expected_id) {
    const auto decoded = robot::decode_message(data);
    if (!decoded || decoded->module_id != expected_id) {
        return std::nullopt;
    }

    IncomingCommand command;
    command.type = decoded->type;
    command.sequence = decoded->sequence;

    if (decoded->type == robot::MessageType::TaskCommand) {
        const auto role = robot::decode_role_payload(decoded->payload);
        if (!role) {
            return std::nullopt;
        }
        command.role = *role;
        return command;
    }

    if (decoded->type == robot::MessageType::TaskAssign) {
        const auto task = robot::decode_task_assign_payload(decoded->payload);
        if (!task) {
            return std::nullopt;
        }
        command.task = *task;
        return command;
    }

    return std::nullopt;
}

std::optional<LowerFeedback> parse_lower_feedback(const std::string& line) {
    std::istringstream in(line);
    std::string tag;
    in >> tag;
    if (tag.empty()) {
        return std::nullopt;
    }

    LowerFeedback feedback;
    if (tag == "ODOM") {
        int x = 0;
        int y = 0;
        double yaw = 0.0;
        int battery = 0;
        if (!(in >> x >> y >> yaw >> battery)) {
            return std::nullopt;
        }
        (void)yaw;
        feedback.x = x;
        feedback.y = y;
        feedback.battery = battery;
        return feedback;
    }

    if (tag == "BAT") {
        int battery = 0;
        if (!(in >> battery)) {
            return std::nullopt;
        }
        feedback.battery = battery;
        return feedback;
    }

    if (tag == "FAULT") {
        feedback.motor_fault = true;
        in >> feedback.fault_code;
        return feedback;
    }

    return std::nullopt;
}

void apply_lower_feedback(Config& config, const LowerFeedback& feedback) {
    if (feedback.x && feedback.y) {
        config.x = *feedback.x;
        config.y = *feedback.y;
    }
    if (feedback.battery) {
        config.battery = std::clamp(*feedback.battery, 0, 100);
    }
}

void send_velocity(robot::SerialPort& serial, int left, int right) {
    std::ostringstream command;
    command << "VEL " << left << ' ' << right;
    serial.write_line(command.str());
    std::cout << "[stm32-tx] " << command.str() << '\n';
}

} // namespace

int main(int argc, char** argv) {
    Config config = parse_args(argc, argv);

    robot::UdpSocket socket;
    if (!socket.open() || !socket.bind(0) || !socket.set_nonblocking()) {
        std::cerr << "failed to start bridge UDP socket\n";
        return 1;
    }

    robot::SerialPort serial;
    if (!serial.open(config.serial_device, config.baud_rate)) {
        return 1;
    }

    std::cout << "stm32_bridge id=" << config.id
              << " master=" << config.master_host << ":" << config.master_port
              << " serial=" << config.serial_device << "@" << config.baud_rate << '\n';

    std::uint32_t sequence = 1;
    robot::Role role = robot::Role::Idle;
    CurrentTask task;
    auto last_heartbeat = Clock::now() - std::chrono::seconds(1);
    auto last_task_report = Clock::now();
    auto last_ping = Clock::now();
    bool lower_feedback_seen = false;
    bool fault_reported = false;

    socket.send_to(config.master_host,
                   config.master_port,
                   make_status_message(robot::MessageType::Register, config, sequence++));
    serial.write_line("PING");

    while (true) {
        const auto now = Clock::now();

        if (std::chrono::duration_cast<std::chrono::milliseconds>(now - last_heartbeat).count() >= 500) {
            if (!lower_feedback_seen) {
                config.battery = std::max(0, config.battery - 1);
            }
            socket.send_to(config.master_host,
                           config.master_port,
                           make_status_message(robot::MessageType::Heartbeat, config, sequence++));
            last_heartbeat = now;
        }

        if (std::chrono::duration_cast<std::chrono::seconds>(now - last_ping).count() >= 2) {
            serial.write_line("PING");
            last_ping = now;
        }

        std::string data;
        robot::UdpEndpoint endpoint;
        while (socket.receive_from(data, endpoint)) {
            const auto command = parse_command(data, config.id);
            if (!command) {
                continue;
            }

            socket.send_to(config.master_host,
                           config.master_port,
                           make_ack_message(config, sequence++, command->sequence));

            if (command->type == robot::MessageType::TaskCommand) {
                role = command->role;
                std::cout << "[bridge] role=" << robot::to_string(role) << '\n';
                if (role == robot::Role::Idle) {
                    serial.write_line("STOP");
                }
            } else if (command->type == robot::MessageType::TaskAssign) {
                task.id = command->task.task_id;
                task.type = command->task.type;
                task.state = robot::TaskState::Running;
                task.progress = 0;
                task.target_x = command->task.target_x;
                task.target_y = command->task.target_y;
                std::cout << "[bridge] task=" << task.id
                          << " target=(" << task.target_x << "," << task.target_y << ")\n";
                send_velocity(serial, config.drive_pwm, config.drive_pwm);
            }
        }

        if (task.id != 0 && task.state == robot::TaskState::Running &&
            std::chrono::duration_cast<std::chrono::milliseconds>(now - last_task_report).count() >= 500) {
            task.progress = static_cast<std::uint16_t>(std::min<int>(100, task.progress + 10));
            if (task.progress >= 100) {
                task.state = robot::TaskState::Done;
                serial.write_line("STOP");
            }
            socket.send_to(config.master_host,
                           config.master_port,
                           make_task_status_message(config, sequence++, task));
            last_task_report = now;
        }

        while (const auto line = serial.read_line()) {
            if (!line->empty()) {
                std::cout << "[stm32-rx] " << *line << '\n';
                const auto feedback = parse_lower_feedback(*line);
                if (!feedback) {
                    continue;
                }
                lower_feedback_seen = true;
                apply_lower_feedback(config, *feedback);
                if (feedback->motor_fault && !fault_reported) {
                    socket.send_to(config.master_host,
                                   config.master_port,
                                   make_fault_message(config, sequence++));
                    std::cout << "[bridge] lower fault reported";
                    if (!feedback->fault_code.empty()) {
                        std::cout << " code=" << feedback->fault_code;
                    }
                    std::cout << '\n';
                    serial.write_line("STOP");
                    task.state = robot::TaskState::Idle;
                    fault_reported = true;
                }
            }
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(20));
    }
}
