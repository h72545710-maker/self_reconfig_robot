#include "robot/protocol.h"
#include "robot/types.h"
#include "robot/udp_socket.h"

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <iostream>
#include <optional>
#include <string>
#include <thread>

namespace {

using Clock = std::chrono::steady_clock;

struct Config {
    std::uint32_t id = 1;
    std::string master_host = "127.0.0.1";
    std::uint16_t master_port = 9000;
    int x = 0;
    int y = 0;
    int battery = 90;
    int duration_sec = 0;
    int fail_after_sec = 0;
    int silent_after_sec = 0;
    int drop_ack_once = 0;
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
        } else if (arg == "--x" && i + 1 < argc) {
            config.x = std::stoi(argv[++i]);
        } else if (arg == "--y" && i + 1 < argc) {
            config.y = std::stoi(argv[++i]);
        } else if (arg == "--battery" && i + 1 < argc) {
            config.battery = std::stoi(argv[++i]);
        } else if (arg == "--duration" && i + 1 < argc) {
            config.duration_sec = std::stoi(argv[++i]);
        } else if (arg == "--fail-after" && i + 1 < argc) {
            config.fail_after_sec = std::stoi(argv[++i]);
        } else if (arg == "--silent-after" && i + 1 < argc) {
            config.silent_after_sec = std::stoi(argv[++i]);
        } else if (arg == "--drop-ack-once" && i + 1 < argc) {
            config.drop_ack_once = std::stoi(argv[++i]);
        }
    }
    return config;
}

std::string make_status_message(robot::MessageType type, const Config& config, std::uint32_t sequence) {
    const robot::StatusPayload payload{config.x, config.y, config.battery};
    return robot::encode_message(type, config.id, sequence, robot::encode_status_payload(payload));
}

std::string make_fault_message(const Config& config, std::uint32_t sequence) {
    return robot::encode_message(robot::MessageType::FaultReport, config.id, sequence, {});
}

std::string make_ack_message(const Config& config, std::uint32_t sequence, std::uint32_t ack_sequence) {
    return robot::encode_message(robot::MessageType::Ack,
                                 config.id,
                                 sequence,
                                 robot::encode_ack_payload(ack_sequence));
}

struct RoleCommand {
    robot::Role role = robot::Role::Idle;
    std::uint32_t sequence = 0;
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

std::string make_task_status_message(const Config& config,
                                     std::uint32_t sequence,
                                     const CurrentTask& task) {
    const robot::TaskStatusPayload payload{task.id, task.state, task.progress};
    return robot::encode_message(robot::MessageType::TaskStatus,
                                 config.id,
                                 sequence,
                                 robot::encode_task_status_payload(payload));
}

} // namespace

int main(int argc, char** argv) {
    Config config = parse_args(argc, argv);

    robot::UdpSocket socket;
    if (!socket.open() || !socket.bind(0) || !socket.set_nonblocking()) {
        std::cerr << "failed to start module UDP socket\n";
        return 1;
    }

    std::cout << "module_node id=" << config.id
              << " master=" << config.master_host << ":" << config.master_port << '\n';

    std::uint32_t sequence = 1;
    socket.send_to(config.master_host,
                   config.master_port,
                   make_status_message(robot::MessageType::Register, config, sequence++));

    robot::Role role = robot::Role::Idle;
    auto last_heartbeat = Clock::now() - std::chrono::seconds(1);
    const auto start = Clock::now();
    bool fault_sent = false;
    bool silent = false;
    CurrentTask task;

    while (true) {
        const auto now = Clock::now();
        const auto elapsed_sec = std::chrono::duration_cast<std::chrono::seconds>(now - start).count();

        if (config.fail_after_sec > 0 && elapsed_sec >= config.fail_after_sec && !fault_sent) {
            const std::string fault = make_fault_message(config, sequence++);
            socket.send_to(config.master_host, config.master_port, fault);
            std::cout << "[module " << config.id << "] fault reported, stop heartbeat\n";
            fault_sent = true;
        }

        if (config.silent_after_sec > 0 && elapsed_sec >= config.silent_after_sec && !silent) {
            std::cout << "[module " << config.id << "] heartbeat stopped silently\n";
            silent = true;
        }

        if (!fault_sent && !silent &&
            std::chrono::duration_cast<std::chrono::milliseconds>(now - last_heartbeat).count() >= 500) {
            if (role == robot::Role::Leader) {
                ++config.x;
            } else if (role == robot::Role::Follower && config.x > 0) {
                --config.x;
            }
            config.battery = std::max(0, config.battery - 1);
            socket.send_to(config.master_host,
                           config.master_port,
                           make_status_message(robot::MessageType::Heartbeat, config, sequence++));

            if (role == robot::Role::Leader && task.id != 0 && task.state == robot::TaskState::Running) {
                task.progress = static_cast<std::uint16_t>(std::min<int>(100, task.progress + 15));
                if (task.progress >= 100) {
                    task.state = robot::TaskState::Done;
                }
                socket.send_to(config.master_host,
                               config.master_port,
                               make_task_status_message(config, sequence++, task));
            }

            last_heartbeat = now;
        }

        std::string data;
        robot::UdpEndpoint endpoint;
        while (socket.receive_from(data, endpoint)) {
            const auto command = parse_command(data, config.id);
            if (!command) {
                continue;
            }

            if (config.drop_ack_once > 0) {
                --config.drop_ack_once;
                std::cout << "[module " << config.id << "] drop ack for seq=" << command->sequence << '\n';
            } else {
                socket.send_to(config.master_host,
                               config.master_port,
                               make_ack_message(config, sequence++, command->sequence));
            }

            if (command->type == robot::MessageType::TaskCommand && command->role != role) {
                role = command->role;
                std::cout << "[module " << config.id << "] role=" << robot::to_string(role) << '\n';
            } else if (command->type == robot::MessageType::TaskAssign) {
                task.id = command->task.task_id;
                task.type = command->task.type;
                task.state = robot::TaskState::Running;
                task.progress = 0;
                task.target_x = command->task.target_x;
                task.target_y = command->task.target_y;
                std::cout << "[module " << config.id << "] task="
                          << task.id << " type=" << robot::task_type_name(task.type)
                          << " target=(" << task.target_x << "," << task.target_y << ")\n";
            }
        }

        if (config.duration_sec > 0 && elapsed_sec >= config.duration_sec) {
            break;
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }

    return 0;
}
