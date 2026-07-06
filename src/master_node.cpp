#include "robot/protocol.h"
#include "robot/types.h"
#include "robot/udp_socket.h"

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <optional>
#include <sstream>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>

namespace {

using Clock = std::chrono::steady_clock;

struct PendingCommand {
    robot::Role role = robot::Role::Idle;
    std::uint32_t sequence = 0;
    Clock::time_point last_sent = Clock::now();
    int retries = 0;
};

struct PendingTaskCommand {
    robot::TaskAssignPayload task{};
    std::uint32_t sequence = 0;
    Clock::time_point last_sent = Clock::now();
    int retries = 0;
};

struct ModuleRecord {
    robot::RobotState state;
    robot::UdpEndpoint endpoint;
    Clock::time_point last_seen = Clock::now();
    std::uint32_t last_sequence = 0;
    robot::Role last_acked_role = robot::Role::Idle;
    std::uint32_t last_acked_task_id = 0;
    std::optional<PendingCommand> pending_role;
    std::optional<PendingTaskCommand> pending_task;
};

struct ActiveTask {
    std::uint32_t id = 1;
    robot::TaskType type = robot::TaskType::Explore;
    robot::Point target{12, 4};
    std::uint32_t assigned_module = 0;
    robot::TaskState state = robot::TaskState::Idle;
    std::uint16_t progress = 0;
};

struct Config {
    std::uint16_t port = 9000;
    int duration_sec = 0;
    std::string state_file = "build/state.json";
};

Config parse_args(int argc, char** argv) {
    Config config;
    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];
        if (arg == "--port" && i + 1 < argc) {
            config.port = static_cast<std::uint16_t>(std::stoi(argv[++i]));
        } else if (arg == "--duration" && i + 1 < argc) {
            config.duration_sec = std::stoi(argv[++i]);
        } else if (arg == "--state-file" && i + 1 < argc) {
            config.state_file = argv[++i];
        }
    }
    return config;
}

std::string json_escape(const std::string& value) {
    std::string out;
    for (char ch : value) {
        switch (ch) {
        case '\\':
            out += "\\\\";
            break;
        case '"':
            out += "\\\"";
            break;
        case '\n':
            out += "\\n";
            break;
        case '\r':
            break;
        default:
            out += ch;
            break;
        }
    }
    return out;
}

void record_event(std::vector<std::string>& events, const std::string& message) {
    events.push_back(message);
    if (events.size() > 30) {
        events.erase(events.begin());
    }
    std::cout << message << '\n';
}

void update_module(std::unordered_map<std::uint32_t, ModuleRecord>& modules,
                   std::uint32_t id,
                   int x,
                   int y,
                   int battery,
                   robot::UdpEndpoint endpoint,
                   std::uint32_t sequence) {
    auto& module = modules[id];
    module.state.id = id;
    module.state.pos = {x, y};
    module.state.battery = battery;
    module.state.status = robot::ModuleStatus::Online;
    module.endpoint = std::move(endpoint);
    module.last_seen = Clock::now();
    module.last_sequence = sequence;
}

void handle_message(const std::string& message,
                    const robot::UdpEndpoint& endpoint,
                    std::unordered_map<std::uint32_t, ModuleRecord>& modules,
                    ActiveTask& active_task,
                    std::vector<std::string>& events) {
    const auto decoded = robot::decode_message(message);
    if (!decoded) {
        std::cout << "[drop] invalid frame from=" << endpoint.host << ":" << endpoint.port << '\n';
        return;
    }

    const auto& frame = *decoded;
    if (frame.type == robot::MessageType::Register || frame.type == robot::MessageType::Heartbeat) {
        const auto status = robot::decode_status_payload(frame.payload);
        if (!status) {
            std::cout << "[drop] bad status payload module=" << frame.module_id << '\n';
            return;
        }
        update_module(modules,
                      frame.module_id,
                      status->x,
                      status->y,
                      status->battery,
                      endpoint,
                      frame.sequence);
        if (frame.type == robot::MessageType::Register) {
            std::ostringstream out;
            out << "[register] module=" << frame.module_id
                << " seq=" << frame.sequence
                << " crc=0x" << std::hex << frame.received_crc << std::dec
                << " from=" << endpoint.host << ":" << endpoint.port;
            record_event(events, out.str());
        }
    } else if (frame.type == robot::MessageType::FaultReport) {
        auto& module = modules[frame.module_id];
        module.state.id = frame.module_id;
        module.state.status = robot::ModuleStatus::Fault;
        module.state.battery = 0;
        module.endpoint = endpoint;
        module.last_seen = Clock::now();
        module.last_sequence = frame.sequence;
        record_event(events,
                     "[fault] module=" + std::to_string(frame.module_id) +
                         " reported fault seq=" + std::to_string(frame.sequence));
    } else if (frame.type == robot::MessageType::Ack) {
        const auto acked_sequence = robot::decode_ack_payload(frame.payload);
        if (!acked_sequence) {
            std::cout << "[drop] bad ack payload module=" << frame.module_id << '\n';
            return;
        }

        auto it = modules.find(frame.module_id);
        if (it == modules.end()) {
            return;
        }

        auto& module = it->second;
        if (module.pending_role && module.pending_role->sequence == *acked_sequence) {
            module.last_acked_role = module.pending_role->role;
            record_event(events,
                         "[ack] module=" + std::to_string(frame.module_id) +
                             " role=" + robot::to_string(module.last_acked_role) +
                             " seq=" + std::to_string(*acked_sequence));
            module.pending_role.reset();
        } else if (module.pending_task && module.pending_task->sequence == *acked_sequence) {
            module.last_acked_task_id = module.pending_task->task.task_id;
            active_task.state = robot::TaskState::Running;
            record_event(events,
                         "[task-ack] module=" + std::to_string(frame.module_id) +
                             " task=" + std::to_string(module.pending_task->task.task_id) +
                             " seq=" + std::to_string(*acked_sequence));
            module.pending_task.reset();
        }
    } else if (frame.type == robot::MessageType::TaskStatus) {
        const auto task_status = robot::decode_task_status_payload(frame.payload);
        if (!task_status) {
            std::cout << "[drop] bad task status payload module=" << frame.module_id << '\n';
            return;
        }
        if (task_status->task_id != active_task.id || frame.module_id != active_task.assigned_module) {
            return;
        }
        active_task.state = task_status->state;
        active_task.progress = task_status->progress;
        record_event(events,
                     "[task-status] module=" + std::to_string(frame.module_id) +
                         " task=" + std::to_string(task_status->task_id) +
                         " state=" + robot::task_state_name(task_status->state) +
                         " progress=" + std::to_string(task_status->progress) + "%");
    }
}

void refresh_status(std::unordered_map<std::uint32_t, ModuleRecord>& modules) {
    const auto now = Clock::now();
    for (auto& [id, module] : modules) {
        (void)id;
        if (module.state.status == robot::ModuleStatus::Fault) {
            continue;
        }
        const auto silent_ms = std::chrono::duration_cast<std::chrono::milliseconds>(now - module.last_seen).count();
        if (silent_ms > 2500) {
            module.state.status = robot::ModuleStatus::Offline;
            module.state.role = robot::Role::Idle;
        }
    }
}

std::uint32_t elect_leader(std::unordered_map<std::uint32_t, ModuleRecord>& modules) {
    std::uint32_t leader_id = 0;
    int best_battery = -1;

    for (auto& [id, module] : modules) {
        module.state.role = robot::Role::Idle;
        if (module.state.status != robot::ModuleStatus::Online) {
            continue;
        }
        if (module.state.battery > best_battery) {
            best_battery = module.state.battery;
            leader_id = id;
        }
    }

    for (auto& [id, module] : modules) {
        if (module.state.status != robot::ModuleStatus::Online) {
            continue;
        }
        module.state.role = (id == leader_id) ? robot::Role::Leader : robot::Role::Follower;
    }

    return leader_id;
}

void send_role_command(robot::UdpSocket& socket, const ModuleRecord& module, const PendingCommand& command) {
    const std::string payload = robot::encode_role_payload(command.role);
    const std::string frame = robot::encode_message(robot::MessageType::TaskCommand,
                                                    module.state.id,
                                                    command.sequence,
                                                    payload);
    socket.send_to(module.endpoint.host, module.endpoint.port, frame);
}

void service_role_commands(robot::UdpSocket& socket,
                           std::unordered_map<std::uint32_t, ModuleRecord>& modules,
                           std::uint32_t& sequence,
                           std::vector<std::string>& events) {
    const auto now = Clock::now();
    constexpr int kRetryTimeoutMs = 600;
    constexpr int kMaxRetries = 5;

    for (auto& [id, module] : modules) {
        (void)id;
        if (module.state.status != robot::ModuleStatus::Online) {
            continue;
        }

        if (module.pending_role && module.pending_role->role != module.state.role) {
            module.pending_role.reset();
        }

        if (!module.pending_role && module.last_acked_role != module.state.role) {
            PendingCommand command;
            command.role = module.state.role;
            command.sequence = sequence++;
            command.last_sent = now;
            command.retries = 0;
            module.pending_role = command;
            send_role_command(socket, module, *module.pending_role);
            record_event(events,
                         "[send] module=" + std::to_string(module.state.id) +
                             " role=" + robot::to_string(command.role) +
                             " seq=" + std::to_string(command.sequence));
            continue;
        }

        if (!module.pending_role) {
            continue;
        }

        const auto elapsed_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                                    now - module.pending_role->last_sent)
                                    .count();
        if (elapsed_ms < kRetryTimeoutMs) {
            continue;
        }

        if (module.pending_role->retries >= kMaxRetries) {
            record_event(events,
                         "[ack-timeout] module=" + std::to_string(module.state.id) +
                             " seq=" + std::to_string(module.pending_role->sequence));
            module.pending_role.reset();
            continue;
        }

        ++module.pending_role->retries;
        module.pending_role->last_sent = now;
        send_role_command(socket, module, *module.pending_role);
        record_event(events,
                     "[retry] module=" + std::to_string(module.state.id) +
                         " role=" + robot::to_string(module.pending_role->role) +
                         " seq=" + std::to_string(module.pending_role->sequence) +
                         " retry=" + std::to_string(module.pending_role->retries));
    }
}

void send_task_command(robot::UdpSocket& socket, const ModuleRecord& module, const PendingTaskCommand& command) {
    const std::string frame = robot::encode_message(robot::MessageType::TaskAssign,
                                                    module.state.id,
                                                    command.sequence,
                                                    robot::encode_task_assign_payload(command.task));
    socket.send_to(module.endpoint.host, module.endpoint.port, frame);
}

void service_task_assignment(robot::UdpSocket& socket,
                             std::unordered_map<std::uint32_t, ModuleRecord>& modules,
                             std::uint32_t leader_id,
                             ActiveTask& active_task,
                             std::uint32_t& sequence,
                             std::vector<std::string>& events) {
    if (leader_id == 0 || active_task.state == robot::TaskState::Done) {
        return;
    }

    const auto leader_it = modules.find(leader_id);
    if (leader_it == modules.end() || leader_it->second.state.status != robot::ModuleStatus::Online) {
        return;
    }

    if (active_task.assigned_module != leader_id) {
        const auto old_module = active_task.assigned_module;
        active_task.assigned_module = leader_id;
        active_task.state = robot::TaskState::Idle;
        active_task.progress = 0;
        auto& leader = leader_it->second;
        leader.last_acked_task_id = 0;
        leader.pending_task.reset();

        if (old_module == 0) {
            record_event(events,
                         "[task-assign] task=" + std::to_string(active_task.id) +
                             " type=" + robot::task_type_name(active_task.type) +
                             " module=" + std::to_string(leader_id));
        } else {
            record_event(events,
                         "[task-reassign] task=" + std::to_string(active_task.id) +
                             " from=" + std::to_string(old_module) +
                             " to=" + std::to_string(leader_id));
        }
    }

    auto& module = modules[active_task.assigned_module];
    if (module.last_acked_task_id == active_task.id && !module.pending_task) {
        return;
    }

    const auto now = Clock::now();
    constexpr int kRetryTimeoutMs = 800;
    constexpr int kMaxRetries = 5;

    if (!module.pending_task) {
        PendingTaskCommand command;
        command.task.task_id = active_task.id;
        command.task.type = active_task.type;
        command.task.target_x = active_task.target.x;
        command.task.target_y = active_task.target.y;
        command.sequence = sequence++;
        command.last_sent = now;
        command.retries = 0;
        module.pending_task = command;
        send_task_command(socket, module, *module.pending_task);
        record_event(events,
                     "[task-send] module=" + std::to_string(module.state.id) +
                         " task=" + std::to_string(active_task.id) +
                         " seq=" + std::to_string(command.sequence));
        return;
    }

    const auto elapsed_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                                now - module.pending_task->last_sent)
                                .count();
    if (elapsed_ms < kRetryTimeoutMs) {
        return;
    }

    if (module.pending_task->retries >= kMaxRetries) {
        record_event(events,
                     "[task-ack-timeout] module=" + std::to_string(module.state.id) +
                         " task=" + std::to_string(active_task.id) +
                         " seq=" + std::to_string(module.pending_task->sequence));
        module.pending_task.reset();
        return;
    }

    ++module.pending_task->retries;
    module.pending_task->last_sent = now;
    send_task_command(socket, module, *module.pending_task);
    record_event(events,
                 "[task-retry] module=" + std::to_string(module.state.id) +
                     " task=" + std::to_string(active_task.id) +
                     " seq=" + std::to_string(module.pending_task->sequence) +
                     " retry=" + std::to_string(module.pending_task->retries));
}

void write_state_json(const std::string& path,
                      const std::unordered_map<std::uint32_t, ModuleRecord>& modules,
                      std::uint32_t leader_id,
                      const ActiveTask& active_task,
                      const std::vector<std::string>& events) {
    std::vector<std::uint32_t> ids;
    ids.reserve(modules.size());
    for (const auto& [id, module] : modules) {
        (void)module;
        ids.push_back(id);
    }
    std::sort(ids.begin(), ids.end());

    std::ofstream out(path, std::ios::trunc);
    if (!out) {
        return;
    }

    out << "{\n";
    out << "  \"leader\": " << leader_id << ",\n";
    out << "  \"map\": {\"width\": 18, \"height\": 10, \"target\": {\"x\": "
        << active_task.target.x << ", \"y\": " << active_task.target.y << "}},\n";
    out << "  \"task\": {\"id\": " << active_task.id
        << ", \"type\": \"" << robot::task_type_name(active_task.type)
        << "\", \"assigned\": " << active_task.assigned_module
        << ", \"state\": \"" << robot::task_state_name(active_task.state)
        << "\", \"progress\": " << active_task.progress
        << ", \"target\": {\"x\": " << active_task.target.x
        << ", \"y\": " << active_task.target.y << "}},\n";
    out << "  \"modules\": [\n";
    for (std::size_t i = 0; i < ids.size(); ++i) {
        const auto& module = modules.at(ids[i]);
        const auto& state = module.state;
        out << "    {\"id\": " << state.id
            << ", \"role\": \"" << robot::to_string(state.role)
            << "\", \"status\": \"" << robot::to_string(state.status)
            << "\", \"x\": " << state.pos.x
            << ", \"y\": " << state.pos.y
            << ", \"battery\": " << state.battery
            << ", \"seq\": " << module.last_sequence
            << ", \"ackedRole\": \"" << robot::to_string(module.last_acked_role)
            << "\"}";
        if (i + 1 < ids.size()) {
            out << ",";
        }
        out << "\n";
    }
    out << "  ],\n";
    out << "  \"events\": [\n";
    for (std::size_t i = 0; i < events.size(); ++i) {
        out << "    \"" << json_escape(events[i]) << "\"";
        if (i + 1 < events.size()) {
            out << ",";
        }
        out << "\n";
    }
    out << "  ]\n";
    out << "}\n";
}

void print_table(const std::unordered_map<std::uint32_t, ModuleRecord>& modules,
                 std::uint32_t leader_id,
                 const ActiveTask& active_task) {
    std::cout << "\n[master] modules=" << modules.size() << " leader=" << leader_id << '\n';
    std::cout << "task=" << active_task.id
              << " type=" << robot::task_type_name(active_task.type)
              << " assigned=" << active_task.assigned_module
              << " state=" << robot::task_state_name(active_task.state)
              << " progress=" << active_task.progress << "%\n";
    for (const auto& [id, module] : modules) {
        const auto& state = module.state;
        std::cout << "module=" << id
                  << " role=" << robot::to_string(state.role)
                  << " status=" << robot::to_string(state.status)
                  << " pos=(" << state.pos.x << "," << state.pos.y << ")"
                  << " battery=" << state.battery
                  << " seq=" << module.last_sequence
                  << " acked_role=" << robot::to_string(module.last_acked_role)
                  << " endpoint=" << module.endpoint.host << ":" << module.endpoint.port << '\n';
    }
}

} // namespace

int main(int argc, char** argv) {
    const Config config = parse_args(argc, argv);

    robot::UdpSocket socket;
    if (!socket.open() || !socket.bind(config.port) || !socket.set_nonblocking()) {
        std::cerr << "failed to start UDP master on port " << config.port << '\n';
        return 1;
    }

    std::cout << "master_node listening on UDP port " << config.port << '\n';

    std::unordered_map<std::uint32_t, ModuleRecord> modules;
    auto last_print = Clock::now();
    const auto start = Clock::now();
    std::uint32_t last_leader = 0;
    std::uint32_t master_sequence = 1;
    ActiveTask active_task;
    std::vector<std::string> events;

    while (true) {
        std::string data;
        robot::UdpEndpoint endpoint;
        while (socket.receive_from(data, endpoint)) {
            handle_message(data, endpoint, modules, active_task, events);
        }

        refresh_status(modules);
        const std::uint32_t leader_id = elect_leader(modules);
        if (leader_id != last_leader) {
            record_event(events,
                         "[leader-change] " + std::to_string(last_leader) +
                             " -> " + std::to_string(leader_id));
            last_leader = leader_id;
        }
        service_role_commands(socket, modules, master_sequence, events);
        service_task_assignment(socket, modules, leader_id, active_task, master_sequence, events);
        write_state_json(config.state_file, modules, leader_id, active_task, events);

        const auto now = Clock::now();
        if (std::chrono::duration_cast<std::chrono::milliseconds>(now - last_print).count() >= 1000) {
            print_table(modules, leader_id, active_task);
            last_print = now;
        }

        if (config.duration_sec > 0 &&
            std::chrono::duration_cast<std::chrono::seconds>(now - start).count() >= config.duration_sec) {
            break;
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    return 0;
}
